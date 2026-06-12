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

const MASTER_JOB_LOCK_MS = 660_000; // slightly above 10 min Python timeout
const MASTER_JOB_RENEW_MS = 30_000;

const createRedisConnection = (label) => {
  if (!env.REDIS_URL) return null;
  const conn = new Redis(env.REDIS_URL, {
    maxRetriesPerRequest: null,
    enableReadyCheck: false,
    enableOfflineQueue: false,
    connectTimeout: 15_000,
    keepAlive: 30_000,
    retryStrategy: (times) => {
      if (times > 20) {
        console.error(`[QUICK MASTER] Redis ${label} giving up after ${times} retries`);
        return null;
      }
      return Math.min(times * 500, 5_000);
    },
    ...(env.REDIS_URL.startsWith("rediss://") ? { tls: {} } : {}),
  });
  conn.on("connect", () => {
    console.log(`[QUICK MASTER] Redis ${label} connected`);
  });
  conn.on("reconnecting", () => {
    console.warn(`[QUICK MASTER] Redis ${label} reconnecting...`);
  });
  return conn;
};

// BullMQ requires separate Redis connections for Queue and Worker.
const queueRedis = createRedisConnection("queue");
const workerRedis = createRedisConnection("worker");

export const masteringQueue = queueRedis
  ? new Queue("mastering", { connection: queueRedis })
  : null;

const pathCache = new Map();
const artworkPathCache = new Map();
const downloadCache = new Map();

const safeUnlink = (filePath) => {
  if (!filePath) return;
  fsp.unlink(filePath).catch(() => {});
};

const sourcePathForMaster = (masterId, sourceName) =>
  path.join(
    MASTER_TMP_DIR,
    `${masterId}-source${path.extname(sourceName) || ".audio"}`,
  );

const artworkPathForMaster = (masterId) => {
  for (const ext of [".png", ".jpg", ".jpeg", ".webp"]) {
    const p = path.join(MASTER_TMP_DIR, `${masterId}-artwork${ext}`);
    if (fs.existsSync(p)) return p;
  }
  return null;
};

/** Wait briefly for enqueue to finish moving uploads onto disk (VPS race guard). */
const waitForSourceFile = async (filePath, attempts = 15, delayMs = 200) => {
  for (let i = 0; i < attempts; i++) {
    if (fs.existsSync(filePath)) return filePath;
    await new Promise((r) => setTimeout(r, delayMs));
  }
  return fs.existsSync(filePath) ? filePath : null;
};

const resolveSourcePath = async (masterId, master) => {
  const onDisk = sourcePathForMaster(masterId, master.sourceName);
  const cached = pathCache.get(masterId);
  if (cached && fs.existsSync(cached)) return cached;
  if (fs.existsSync(onDisk)) return onDisk;

  if (master.sourceUrl?.startsWith("local://pending")) {
    const ready = await waitForSourceFile(onDisk);
    if (ready) return ready;
    throw new Error("Source file not ready on server. Please try again.");
  }

  if (!master.sourceUrl || master.sourceUrl.startsWith("local://")) {
    throw new Error("Source file not found on server.");
  }

  const sourceDownloadUrl = await getDownloadUrl(master.sourceUrl);
  const sourceResponse = await fetch(sourceDownloadUrl);
  if (!sourceResponse.ok) throw new Error("Failed to download source audio");
  const buf = Buffer.from(await sourceResponse.arrayBuffer());
  await fsp.writeFile(onDisk, buf);
  return onDisk;
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
    if (fs.existsSync(cachedPath)) {
      try {
        const buf = await fsp.readFile(cachedPath);
        console.log("[QUICK MASTER] Artwork loaded from job cache:", buf.length, "bytes");
        return buf;
      } catch (err) {
        console.warn("[QUICK MASTER] Failed to read cached artwork:", err.message);
      }
    }
  }

  const onDiskArt = artworkPathForMaster(masterId);
  if (onDiskArt) {
    try {
      const buf = await fsp.readFile(onDiskArt);
      console.log("[QUICK MASTER] Artwork loaded from disk:", buf.length, "bytes");
      return buf;
    } catch (err) {
      console.warn("[QUICK MASTER] Failed to read artwork from disk:", err.message);
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

const logQueueHealth = async (label) => {
  if (!masteringQueue) return;
  try {
    const counts = await masteringQueue.getJobCounts(
      "waiting",
      "active",
      "delayed",
      "failed",
      "paused",
    );
    console.log(`[QUICK MASTER] Redis queue ${label}:`, counts);
    return counts;
  } catch (err) {
    console.error("[QUICK MASTER] Redis queue health check failed:", err.message);
    return null;
  }
};

/** Ghost "active" jobs in Redis block worker concurrency until their lock expires. */
const recoverOrphanedActiveJobs = async () => {
  if (!masteringQueue) return;
  try {
    const activeJobs = await masteringQueue.getJobs(["active"], 0, 20);
    if (!activeJobs.length) return;

    const now = Date.now();
    for (const job of activeJobs) {
      const startedAt = job.processedOn || job.timestamp || now;
      const ageMs = now - startedAt;
      if (ageMs < 120_000) continue;

      console.warn(
        "[QUICK MASTER] Recovering orphaned Redis job:",
        job.id,
        job.data?.masterId,
        `(${Math.round(ageMs / 1000)}s in active)`,
      );

      try {
        await job.moveToWait();
        console.log("[QUICK MASTER] Re-queued orphaned job:", job.data?.masterId);
      } catch (moveErr) {
        console.warn("[QUICK MASTER] moveToWait failed, removing job:", moveErr.message);
        await job.remove().catch(() => {});
        if (job.data?.masterId) {
          await prisma.master.update({
            where: { id: job.data.masterId },
            data: {
              status: "FAILED",
              error: "Mastering interrupted — please try again",
            },
          }).catch(() => {});
        }
      }
    }
  } catch (err) {
    console.error("[QUICK MASTER] Orphan job recovery failed:", err.message);
  }
};

if (workerRedis) {
  const worker = new Worker(
    "mastering",
    async (job) => {
      const { masterId } = job.data;
      console.log("[QUICK MASTER] Processing mastering job for master ID:", masterId, `(job ${job.id})`);

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

        srcPath = await resolveSourcePath(masterId, master);
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

        pathCache.delete(masterId);
        artworkPathCache.delete(masterId);
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
    {
      connection: workerRedis,
      concurrency: env.WORKER_CONCURRENCY,
      drainDelay: 200,
      lockDuration: MASTER_JOB_LOCK_MS,
      lockRenewTime: MASTER_JOB_RENEW_MS,
      maxStalledCount: 3,
      stalledInterval: 30_000,
    },
  );

  worker.on("active", (job) => {
    console.log("[QUICK MASTER] Job active:", job.data?.masterId, `(job ${job.id})`);
  });

  worker.on("failed", async (job, error) => {
    if (!job) return;
    console.error("[QUICK MASTER] Job failed for", job.data?.masterId, error?.message || error);
    await prisma.master.update({
      where: { id: job.data.masterId },
      data: { status: "FAILED", error: error.message },
    });
  });

  worker.on("ready", async () => {
    console.log("[QUICK MASTER] BullMQ worker ready (Redis connected)");
    await recoverOrphanedActiveJobs();
    await logQueueHealth("on ready");
  });

  worker.on("stalled", (jobId) => {
    console.warn("[QUICK MASTER] Redis job stalled (lock lost / worker busy):", jobId);
  });

  worker.on("error", (err) => {
    console.error("[QUICK MASTER] BullMQ worker error:", err.message);
  });

  workerRedis.on("error", (err) => {
    console.error("[QUICK MASTER] Redis worker connection error:", err.message);
  });

  queueRedis?.on("error", (err) => {
    console.error("[QUICK MASTER] Redis queue connection error:", err.message);
  });

  console.log("[QUICK MASTER] BullMQ worker started");
} else {
  console.warn("[QUICK MASTER] REDIS_URL not set — mastering queue disabled");
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
    console.log("[QUICK MASTER] Source cached for job:", dest);
  } else if (sourcePath) {
    console.warn("[QUICK MASTER] Source path missing on disk:", sourcePath);
  } else {
    console.warn("[QUICK MASTER] No source path for job:", masterId);
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

  try {
    const job = await masteringQueue.add(
      "process",
      { masterId },
      {
        jobId: masterId,
        removeOnComplete: 100,
        removeOnFail: 50,
        attempts: 2,
        backoff: { type: "fixed", delay: 5000 },
      },
    );
    console.log("[QUICK MASTER] Job queued:", masterId, `(job ${job.id})`);
  } catch (err) {
    if (String(err.message || err).includes("Job already exists")) {
      console.warn("[QUICK MASTER] Job already in Redis queue:", masterId);
    } else {
      throw err;
    }
  }
  logQueueHealth("after enqueue").catch(() => {});
};

export const getLocalDownloadPath = (masterId) => downloadCache.get(masterId);
