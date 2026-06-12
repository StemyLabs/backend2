import { Queue, Worker } from "bullmq";
import Redis from "ioredis";
import { openAsBlob } from "node:fs";
import { Readable } from "stream";
import { env } from "../config/env.js";
import { prisma } from "../lib/prisma.js";
import { sendEmail } from "../utils/email.js";
import { masterReadyEmail } from "../utils/email-templates.js";
import { createAccessToken } from "../utils/tokens.js";
import { MASTER_TMP_DIR, getMasterOutputPath } from "../utils/master-temp.js";
import { getDownloadUrl, uploadStream, readLocalStorage } from "./storage.service.js";
import fs from "fs";
import fsp from "fs/promises";
import path from "path";

const redisConnection = env.REDIS_URL
  ? new Redis(env.REDIS_URL, { maxRetriesPerRequest: null })
  : null;

export const masteringQueue = redisConnection
  ? new Queue("mastering", { connection: redisConnection })
  : null;

const pathCache = new Map();
const artworkPathCache = new Map();
const downloadCache = new Map();

const safeUnlink = (filePath) => {
  if (!filePath) return;
  fsp.unlink(filePath).catch(() => {});
};

const updateProgress = async (masterId, progress, label) => {
  try {
    const master = await prisma.master.findUnique({ where: { id: masterId }, select: { metadata: true } });
    const meta = master?.metadata || {};
    await prisma.master.update({
      where: { id: masterId },
      data: { metadata: { ...meta, progress, progressLabel: label } },
    });
  } catch (err) {
    console.warn("[PROGRESS] Failed to update progress for", masterId, err.message);
  }
};

const METADATA_SKIP_KEYS = new Set(["artworkUrl", "progress", "progressLabel"]);

const cleanMetadataForEngine = (metadata) => {
  if (!metadata || typeof metadata !== "object") return null;
  const cleaned = {};
  for (const [key, value] of Object.entries(metadata)) {
    if (METADATA_SKIP_KEYS.has(key)) continue;
    if (value == null || String(value).trim() === "") continue;
    cleaned[key] = value;
  }
  return Object.keys(cleaned).length ? cleaned : null;
};

const buildEngineMetadata = (metadata) => {
  const cleaned = cleanMetadataForEngine(metadata) || {};
  // Keep artworkUrl for Python fallback fetch if multipart artwork is missing.
  if (metadata?.artworkUrl) {
    cleaned.artworkUrl = metadata.artworkUrl;
  }
  return Object.keys(cleaned).length ? cleaned : null;
};

const loadArtworkBuffer = async (master, masterId) => {
  const cachedPath = artworkPathCache.get(masterId);
  if (cachedPath) {
    artworkPathCache.delete(masterId);
    if (fs.existsSync(cachedPath)) {
      try {
        const buf = await fsp.readFile(cachedPath);
        await fsp.unlink(cachedPath).catch(() => {});
        console.log("[QUICK MASTER] Artwork loaded from job cache:", buf.length, "bytes");
        return buf;
      } catch (err) {
        console.warn("[QUICK MASTER] Failed to read cached artwork:", err.message);
      }
    }
  }

  const artUrl = master.metadata?.artworkUrl;
  if (!artUrl) {
    console.log("[QUICK MASTER] No artwork URL on master record");
    return null;
  }

  try {
    if (artUrl.startsWith("local://")) {
      const buf = await readLocalStorage(artUrl);
      if (buf) {
        console.log("[QUICK MASTER] Artwork loaded from local storage:", buf.length, "bytes");
        return buf;
      }
      console.warn("[QUICK MASTER] Local artwork missing:", artUrl);
      return null;
    }

    // Public R2 URLs are directly fetchable; signed URL is fallback.
    if (artUrl.startsWith("http://") || artUrl.startsWith("https://")) {
      const directResp = await fetch(artUrl);
      if (directResp.ok) {
        const buf = Buffer.from(await directResp.arrayBuffer());
        console.log("[QUICK MASTER] Artwork fetched (public URL):", buf.length, "bytes");
        return buf;
      }
    }

    const artSignedUrl = await getDownloadUrl(artUrl);
    const artResp = await fetch(artSignedUrl);
    if (artResp.ok) {
      const buf = Buffer.from(await artResp.arrayBuffer());
      console.log("[QUICK MASTER] Artwork downloaded (signed URL):", buf.length, "bytes");
      return buf;
    }
    console.warn("[QUICK MASTER] Artwork download failed:", artResp.status, artResp.statusText);
  } catch (artErr) {
    console.warn("[QUICK MASTER] Failed to download artwork:", artErr.message);
  }

  return null;
};

const detectImageMime = (buf) => {
  if (!buf || buf.byteLength < 4) return "image/jpeg";
  const b = Buffer.from(buf);
  if (b[0] === 0x89 && b[1] === 0x50 && b[2] === 0x4e && b[3] === 0x47) return "image/png";
  return "image/jpeg";
};

/** Native FormData + file Blob (undici fetch; npm form-data streams break Readable.toWeb). */
const buildMasterFormData = async ({
  srcPath,
  sourceName,
  sourceMime,
  genre,
  metadata,
  artBuf,
}) => {
  const formData = new FormData();
  const mime = sourceMime || "application/octet-stream";

  let fileBlob;
  try {
    fileBlob = await openAsBlob(srcPath, { type: mime });
  } catch {
    const buf = await fsp.readFile(srcPath);
    fileBlob = new Blob([buf], { type: mime });
  }
  formData.append("file", fileBlob, sourceName);
  formData.append("genre", genre);

  if (metadata) {
    const metaStr =
      typeof metadata === "string" ? metadata : JSON.stringify(metadata);
    formData.append("metadata", metaStr);
  }

  if (artBuf) {
    const artMime = detectImageMime(artBuf);
    const ext = artMime === "image/png" ? "png" : "jpg";
    formData.append("artwork", new Blob([artBuf], { type: artMime }), `cover.${ext}`);
  }

  return formData;
};

if (redisConnection) {
  const worker = new Worker(
    "mastering",
    async (job) => {
      const { masterId } = job.data;
      console.log("[QUICK MASTER] Processing mastering job for master ID:", masterId);

      const master = await prisma.master.findUnique({
        where: { id: masterId },
        include: { user: true },
      });
      if (!master) {
        console.error("[QUICK MASTER] Master not found:", masterId);
        return;
      }

      let srcPath = null;

      try {
        const T = (label) => {
          const t = Date.now();
          return [t, label];
        };
        let marks = [];
        marks.push(T("start"));

        await updateProgress(masterId, 45, "Reading source file...");
        await prisma.master.update({
          where: { id: masterId },
          data: { status: "PROCESSING" },
        });
        await updateProgress(masterId, 50, "Preparing source...");

        srcPath = pathCache.get(masterId);
        if (srcPath) pathCache.delete(masterId);

        if (!srcPath || !fs.existsSync(srcPath)) {
          const sourceDownloadUrl = await getDownloadUrl(master.sourceUrl);
          marks.push(T("signed_url"));
          srcPath = path.join(
            MASTER_TMP_DIR,
            `${masterId}-source${path.extname(master.sourceName) || ".audio"}`,
          );
          const sourceResponse = await fetch(sourceDownloadUrl);
          if (!sourceResponse.ok) throw new Error("Failed to download source audio");
          const buf = Buffer.from(await sourceResponse.arrayBuffer());
          await fsp.writeFile(srcPath, buf);
        }
        marks.push(T("get_src"));
        await updateProgress(masterId, 55, "Source loaded — sending to engine...");

        const srcStat = await fsp.stat(srcPath);
        if (srcStat.size > 100 * 1024 * 1024) {
          throw new Error("File too large. Maximum size is 100MB");
        }

        let artBuf = await loadArtworkBuffer(master, masterId);

        const engineMetadata = buildEngineMetadata(master.metadata);
        const formData = await buildMasterFormData({
          srcPath,
          sourceName: master.sourceName,
          sourceMime: master.sourceMime,
          genre: master.genre,
          metadata: engineMetadata,
          artBuf,
        });

        console.log(
          "[QUICK MASTER] Sending to engine — audio=%d bytes, artwork=%s, metadata=%s",
          srcStat.size,
          artBuf ? `${artBuf.length} bytes` : "none",
          engineMetadata ? Object.keys(engineMetadata).join(", ") : "none",
        );

        await updateProgress(masterId, 60, "Mastering engine processing...");

        console.log(
          "[QUICK MASTER] POST /master — file=%s size=%d bytes",
          master.sourceName,
          srcStat.size,
        );

        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 600000);

        let pythonResponse;
        try {
          pythonResponse = await fetch(`${env.PYTHON_ENGINE_URL}/master`, {
            method: "POST",
            body: formData,
            signal: controller.signal,
          });
        } catch (fetchError) {
          clearTimeout(timeoutId);
          throw new Error(
            fetchError.name === "AbortError"
              ? "Python engine request timed out after 10 minutes"
              : `Cannot connect to Python engine: ${fetchError.message}`,
          );
        }
        clearTimeout(timeoutId);
        marks.push(T("python_done"));
        await updateProgress(masterId, 85, "Writing mastered output...");

        if (!pythonResponse.ok) {
          const errorText = await pythonResponse.text();
          throw new Error(`Python Engine Error: ${pythonResponse.statusText} - ${errorText}`);
        }

        const lufs = parseFloat(pythonResponse.headers.get("X-Lufs-Actual")) || -14;
        const dbtp = parseFloat(pythonResponse.headers.get("X-Tp-Actual")) || -1;
        const dr = parseFloat(pythonResponse.headers.get("X-DR-Actual")) || 6;
        const duration = parseFloat(pythonResponse.headers.get("X-Duration-Actual")) || 0;
        const pyTime = pythonResponse.headers.get("X-Processing-Time-Ms");

        const outputKey = `masters/${master.userId}/${Date.now()}-mastered-${master.sourceName}`;
        const tmpPath = getMasterOutputPath(masterId);
        const outputLength = parseInt(pythonResponse.headers.get("content-length"), 10) || undefined;

        const webStream = pythonResponse.body;
        const nodeStream = Readable.fromWeb(webStream);

        const fileStream = fs.createWriteStream(tmpPath);
        nodeStream.pipe(fileStream);

        await new Promise((resolve, reject) => {
          fileStream.on("finish", resolve);
          fileStream.on("error", reject);
        });
        marks.push(T("write_local"));
        await updateProgress(masterId, 95, "Finalizing...");

        downloadCache.set(masterId, tmpPath);

        await prisma.master.update({
          where: { id: masterId },
          data: {
            status: "COMPLETE",
            completedAt: new Date(),
            lufs,
            dbtp,
            dr,
            duration,
            // Local path is available immediately; R2 URL replaces this after background upload.
            outputUrl: `local://${masterId}.wav`,
          },
        });
        marks.push(T("db_update"));

        const archiveSourcePath = srcPath;
        (async () => {
          try {
            if (
              master.sourceUrl?.startsWith("local://") &&
              archiveSourcePath &&
              fs.existsSync(archiveSourcePath)
            ) {
              const sourceKey = `masters/${master.userId}/${Date.now()}-${master.sourceName}`;
              const archStat = await fsp.stat(archiveSourcePath);
              const sourceUrl = await uploadStream({
                key: sourceKey,
                stream: fs.createReadStream(archiveSourcePath),
                contentType: master.sourceMime || "application/octet-stream",
                contentLength: archStat.size,
              });
              await prisma.master.update({
                where: { id: masterId },
                data: { sourceUrl },
              });
              console.log("[QUICK MASTER] Source archived to R2:", sourceUrl);
            }
          } catch (uploadErr) {
            console.warn("[QUICK MASTER] Background source R2 upload failed:", uploadErr.message);
          }
        })();

        (async () => {
          try {
            const outStat = await fsp.stat(tmpPath);
            const result = await uploadStream({
              key: outputKey,
              stream: fs.createReadStream(tmpPath),
              contentType: "audio/wav",
              contentLength: outStat.size,
            });
            await prisma.master.update({
              where: { id: masterId },
              data: { outputUrl: result },
            });

            setTimeout(() => {
              downloadCache.delete(masterId);
              safeUnlink(tmpPath);
            }, 300000);
          } catch (uploadErr) {
            console.error("[QUICK MASTER] Background R2 output upload failed:", uploadErr);
          }
        })();

        if (master.user?.email) {
          const downloadToken = createAccessToken(master.userId);
          const apiBaseUrl = env.APP_BASE_URL.replace(/\/+$/, "");
          const frontendUrl = env.FRONTEND_URL.replace(/\/+$/, "");
          const downloadUrl = `${apiBaseUrl}/api/masters/${masterId}/download?token=${encodeURIComponent(downloadToken)}`;
          const dashboardUrl = `${frontendUrl}/pages/profile.html`;
          await sendEmail({
            to: master.user.email,
            subject: "Your Stemy master is ready — Download Now",
            html: masterReadyEmail(master.sourceName, downloadUrl, dashboardUrl),
          });
        }

        const fmt = (a, b) => `${((b[0] - a[0]) / 1000).toFixed(1)}s`;
        const srcMB = (srcStat.size / 1024 / 1024).toFixed(1);
        const outMB = outputLength ? (outputLength / 1024 / 1024).toFixed(1) : "?";
        console.log(`\n═══ MASTER TIMINGS ═══`);
        console.log(`  Get source     ${fmt(marks[0], marks[1])}  (${srcMB} MB)`);
        console.log(`  Python engine  ${fmt(marks[1], marks[2])}  (py=${(parseInt(pyTime || 0, 10) / 1000).toFixed(1)}s)`);
        console.log(`  Write local    ${fmt(marks[2], marks[3])}  (${outMB} MB)`);
        console.log(`  DB update      ${fmt(marks[3], marks[4])}`);
        console.log(`  USER READY     ${fmt(marks[0], marks[4])}`);
        console.log(`  (source + output R2 upload in parallel / background)`);
        console.log(`═══════════════════════════════\n`);
      } catch (error) {
        console.error(`Mastering Job Failed for ${masterId}:`, error);
        throw error;
      } finally {
        // Keep source until background R2 archive finishes (~seconds after COMPLETE)
        if (srcPath && srcPath.includes(`${masterId}-source`)) {
          setTimeout(() => safeUnlink(srcPath), 120_000);
        }
      }
    },
    { connection: redisConnection, concurrency: env.WORKER_CONCURRENCY, drainDelay: 200 },
  );

  worker.on("failed", async (job, error) => {
    if (!job) return;
    await prisma.master.update({
      where: { id: job.data.masterId },
      data: { status: "FAILED", error: error.message },
    });
  });
}

export const enqueueMasteringJob = async (masterId, sourcePath, artworkPath = null) => {
  if (sourcePath && fs.existsSync(sourcePath)) {
    const ext = path.extname(sourcePath) || ".audio";
    const dest = path.join(MASTER_TMP_DIR, `${masterId}-source${ext}`);
    try {
      await fsp.rename(sourcePath, dest);
    } catch {
      await fsp.copyFile(sourcePath, dest);
      await fsp.unlink(sourcePath).catch(() => {});
    }
    pathCache.set(masterId, dest);
  }

  if (artworkPath && fs.existsSync(artworkPath)) {
    const ext = path.extname(artworkPath) || ".jpg";
    const dest = path.join(MASTER_TMP_DIR, `${masterId}-artwork${ext}`);
    try {
      await fsp.rename(artworkPath, dest);
    } catch {
      await fsp.copyFile(artworkPath, dest);
      await fsp.unlink(artworkPath).catch(() => {});
    }
    artworkPathCache.set(masterId, dest);
    console.log("[QUICK MASTER] Artwork cached for job:", dest);
  }

  if (!masteringQueue) {
    await prisma.master.update({
      where: { id: masterId },
      data: {
        status: "COMPLETE",
        outputUrl: (await prisma.master.findUnique({ where: { id: masterId } }))
          ?.sourceUrl,
        completedAt: new Date(),
      },
    });
    return;
  }

  await masteringQueue.add("process", { masterId });
};

export const getLocalDownloadPath = (masterId) => downloadCache.get(masterId);
