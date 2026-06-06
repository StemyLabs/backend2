import { Queue, Worker } from "bullmq";
import Redis from "ioredis";
import { openAsBlob } from "node:fs";
import { Readable } from "stream";
import { env } from "../config/env.js";
import { pythonHttpTimeoutMs, runLocalMaster } from "./local-master.js";
import { prisma } from "../lib/prisma.js";
import { sendEmail } from "../utils/email.js";
import { masterReadyEmail } from "../utils/email-templates.js";
import { createAccessToken } from "../utils/tokens.js";
import {
  findStagedMasterSource,
  getMasterTmpDir,
  masterSourcePath,
  stageMasterSource,
} from "../utils/master-temp.js";
import { getDownloadUrl, uploadStream } from "./storage.service.js";
import fs from "fs";
import fsp from "fs/promises";
import path from "path";

const redisConnection = env.REDIS_URL
  ? new Redis(env.REDIS_URL, { maxRetriesPerRequest: null })
  : null;

export const masteringQueue = redisConnection
  ? new Queue("mastering", { connection: redisConnection })
  : null;

const downloadCache = new Map();
/** Serialize inline masters (one at a time on this Node process). */
let inlineMasterChain = Promise.resolve();
const useBullMaster = process.env.STEMY_USE_BULL_MASTER === "1";

const safeUnlink = (filePath) => {
  if (!filePath) return;
  fsp.unlink(filePath).catch(() => {});
};

/** Native FormData + file Blob (undici fetch; npm form-data streams break Readable.toWeb). */
const buildMasterFormData = async ({
  srcPath,
  sourceName,
  sourceMime,
  genre,
  metadata,
  artBuf,
  artworkUrl,
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
    const artUrl = artworkUrl || "";
    const artMime = artUrl.endsWith(".png") ? "image/png" : "image/jpeg";
    const ext = artMime === "image/png" ? "png" : "jpg";
    formData.append("artwork", new Blob([artBuf], { type: artMime }), `cover.${ext}`);
  }

  return formData;
};

const failMaster = async (masterId, message) => {
  await prisma.master.update({
    where: { id: masterId },
    data: { status: "FAILED", error: message },
  });
};

const resolveSourcePath = async (masterId, master) => {
  const tmpDir = getMasterTmpDir();
  const expectedSource = masterSourcePath(masterId, master.sourceName);
  if (fs.existsSync(expectedSource)) {
    return expectedSource;
  }
  const staged = await findStagedMasterSource(masterId);
  if (staged) {
    return staged;
  }
  const listing = await fsp.readdir(tmpDir).catch(() => []);
  console.error(
    "[QUICK MASTER] Source missing for %s — expected %s — dir %s has: %s",
    masterId,
    expectedSource,
    tmpDir,
    listing.filter((n) => n.includes(masterId.slice(0, 8))).join(", ") || "(none)",
  );
  return null;
};

export const processMasteringJob = async (masterId) => {
  console.log("[QUICK MASTER] Processing mastering job for master ID:", masterId);
  console.log("[QUICK MASTER] Temp dir:", getMasterTmpDir());

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
    await prisma.master.update({
      where: { id: masterId },
      data: { status: "PROCESSING" },
    });

    srcPath = await resolveSourcePath(masterId, master);

    if (!srcPath) {
      if (master.sourceUrl?.startsWith("local://pending")) {
        throw new Error(
          "Source file was not found on the server. Re-upload the track and try again.",
        );
      }
      const sourceDownloadUrl = await getDownloadUrl(master.sourceUrl);
      if (sourceDownloadUrl.startsWith("local://")) {
        throw new Error(
          "Invalid local source URL; re-upload the track and try again.",
        );
      }
      marks.push(T("signed_url"));
      srcPath = masterSourcePath(masterId, master.sourceName);
      const sourceResponse = await fetch(sourceDownloadUrl);
      if (!sourceResponse.ok) {
        throw new Error("Failed to download source audio");
      }
      const buf = Buffer.from(await sourceResponse.arrayBuffer());
      await fsp.writeFile(srcPath, buf);
    }

    console.log("[QUICK MASTER] Using source file: %s", srcPath);
    marks.push(T("get_src"));

    const srcStat = await fsp.stat(srcPath);
    if (srcStat.size > 100 * 1024 * 1024) {
      throw new Error("File too large. Maximum size is 100MB");
    }

    let artBuf = null;
    if (master.metadata?.artworkUrl) {
      try {
        const artSignedUrl = await getDownloadUrl(master.metadata.artworkUrl);
        const artResp = await fetch(artSignedUrl);
        if (artResp.ok) {
          artBuf = await artResp.arrayBuffer();
          console.log("[QUICK MASTER] Artwork downloaded:", artBuf.byteLength, "bytes");
        }
      } catch (artErr) {
        console.warn("[QUICK MASTER] Failed to download artwork:", artErr.message);
      }
    }

    const outExt = (process.env.STEMY_OUTPUT_EXT || ".flac").toLowerCase();
    const tmpPath = path.join(getMasterTmpDir(), `${masterId}${outExt}`);

    let lufs = -14;
    let dbtp = -1;
    let dr = 6;
    let duration = 0;
    let pyTime = null;
    let outputLength;

    const useLocalCli = env.PYTHON_USE_LOCAL_CLI === true;

    if (useLocalCli) {
          const localMode = (process.env.PYTHON_LOCAL_MODE || "http").toLowerCase();
          console.log(
            "[QUICK MASTER] Local %s — file=%s size=%d bytes",
            localMode === "cli" ? "CLI" : "HTTP",
            master.sourceName,
            srcStat.size,
          );
          const httpTimeoutMs = pythonHttpTimeoutMs(srcStat.size);
          console.log(
            `[QUICK MASTER] Python HTTP timeout ${Math.round(httpTimeoutMs / 1000)}s (${(srcStat.size / (1024 * 1024)).toFixed(1)} MB)`,
          );
          const { analysis, outputPath } = await runLocalMaster({
            inputPath: srcPath,
            outputPath: tmpPath,
            genre: master.genre,
            fileSizeBytes: srcStat.size,
            timeoutMs: httpTimeoutMs,
          });
          lufs = analysis.lufs ?? lufs;
          dbtp = analysis.dbtp ?? dbtp;
          dr = analysis.dr ?? dr;
          duration = analysis.duration ?? duration;
          pyTime = String(
            analysis.processing_ms ??
              Math.round((analysis.elapsed_sec || 0) * 1000),
          );
          const outStat = await fsp.stat(outputPath);
          outputLength = outStat.size;
          marks.push(T("python_done"));
          marks.push(T("write_local"));
        } else {
          const formData = await buildMasterFormData({
            srcPath,
            sourceName: master.sourceName,
            sourceMime: master.sourceMime,
            genre: master.genre,
            metadata: master.metadata,
            artBuf,
            artworkUrl: master.metadata?.artworkUrl,
          });

          console.log(
            "[QUICK MASTER] POST /master — file=%s size=%d bytes",
            master.sourceName,
            srcStat.size,
          );

          const controller = new AbortController();
          const timeoutId = setTimeout(() => controller.abort(), 120000);

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
                ? "Python engine request timed out after 120 seconds"
                : `Cannot connect to Python engine: ${fetchError.message}`,
            );
          }
          clearTimeout(timeoutId);
          marks.push(T("python_done"));

          if (!pythonResponse.ok) {
            const errorText = await pythonResponse.text();
            throw new Error(`Python Engine Error: ${pythonResponse.statusText} - ${errorText}`);
          }

          lufs = parseFloat(pythonResponse.headers.get("X-Lufs-Actual")) || -14;
          dbtp = parseFloat(pythonResponse.headers.get("X-Tp-Actual")) || -1;
          dr = parseFloat(pythonResponse.headers.get("X-DR-Actual")) || 6;
          duration = parseFloat(pythonResponse.headers.get("X-Duration-Actual")) || 0;
          pyTime = pythonResponse.headers.get("X-Processing-Time-Ms");
          const outFmt = pythonResponse.headers.get("X-Output-Format") || "wav";
          const httpExt = outFmt === "flac" ? ".flac" : ".wav";
          const httpTmp = path.join(getMasterTmpDir(), `${masterId}${httpExt}`);
          outputLength = parseInt(pythonResponse.headers.get("content-length"), 10) || undefined;

          const nodeStream = Readable.fromWeb(pythonResponse.body);
          const fileStream = fs.createWriteStream(httpTmp);
          nodeStream.pipe(fileStream);

          await new Promise((resolve, reject) => {
            fileStream.on("finish", resolve);
            fileStream.on("error", reject);
          });
          if (httpTmp !== tmpPath) {
            await fsp.rename(httpTmp, tmpPath).catch(() => {});
          }
          marks.push(T("write_local"));
        }

        const outBase =
          master.sourceName?.replace(/\.[^.]+$/, "") || "track";
        const outputKey = `masters/${master.userId}/${Date.now()}-mastered-${outBase}${outExt}`;

        downloadCache.set(masterId, tmpPath);

        await prisma.master.update({
          where: { id: masterId },
          data: { status: "COMPLETE", completedAt: new Date(), lufs, dbtp, dr, duration },
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
              contentType: outExt === ".flac" ? "audio/flac" : "audio/wav",
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
    await failMaster(masterId, error.message);
    throw error;
  } finally {
    if (srcPath && srcPath.includes(`${masterId}-source`)) {
      setTimeout(() => safeUnlink(srcPath), 120_000);
    }
  }
};

if (redisConnection && useBullMaster) {
  const worker = new Worker(
    "mastering",
    async (job) => processMasteringJob(job.data.masterId),
    {
      connection: redisConnection,
      drainDelay: 200,
      concurrency: 1,
      lockDuration: 300_000,
    },
  );

  worker.on("failed", async (job, error) => {
    if (!job) return;
    console.error(
      "[QUICK MASTER] Bull worker failed job %s: %s",
      job.data.masterId,
      error?.message,
    );
  });
  console.log("[QUICK MASTER] BullMQ mastering worker enabled (STEMY_USE_BULL_MASTER=1)");
} else {
  console.log(
    "[QUICK MASTER] Inline mastering enabled (default on VPS — no Bull queue delay)",
  );
}

export const enqueueMasteringJob = async (
  masterId,
  sourcePath,
  sourceName = "",
) => {
  const staged = await stageMasterSource(masterId, sourcePath, sourceName);
  if (!staged) {
    const msg =
      "Could not save upload on server disk. Check STEMY_TEMP_DIR permissions and disk space.";
    await prisma.master.update({
      where: { id: masterId },
      data: { status: "FAILED", error: msg },
    });
    throw new Error(msg);
  }
  console.log("[QUICK MASTER] Source staged for %s → %s", masterId, staged);

  if (!masteringQueue) {
    await processMasteringJob(masterId);
    return;
  }

  if (useBullMaster && masteringQueue) {
    const job = await masteringQueue.add(
      "process",
      { masterId },
      { removeOnComplete: 200, removeOnFail: 100 },
    );
    console.log("[QUICK MASTER] Bull job %s queued for master %s", job.id, masterId);
    return;
  }

  inlineMasterChain = inlineMasterChain
    .then(() => processMasteringJob(masterId))
    .catch((err) => {
      console.error("[QUICK MASTER] Inline mastering error:", err.message);
    });
  console.log("[QUICK MASTER] Inline mastering queued for %s", masterId);
};

export const getLocalDownloadPath = (masterId) => downloadCache.get(masterId);
