import { Queue, Worker } from "bullmq";
import Redis from "ioredis";
import { env } from "../config/env.js";
import { prisma } from "../lib/prisma.js";
import { sendEmail } from "../utils/email.js";
import { Readable } from "stream";
import { getDownloadUrl, uploadBuffer } from "./storage.service.js";
import fs from "fs";
import path from "path";
import os from "os";

const TMP_DIR = path.join(os.tmpdir(), "stemy-masters");
if (!fs.existsSync(TMP_DIR)) fs.mkdirSync(TMP_DIR, { recursive: true });
export const SOURCE_DIR = path.join(os.tmpdir(), "stemy-sources");

const redisConnection = env.REDIS_URL
  ? new Redis(env.REDIS_URL, { maxRetriesPerRequest: null })
  : null;

export const masteringQueue = redisConnection
  ? new Queue("mastering", { connection: redisConnection })
  : null;

// Download cache — maps masterId to local temp file path for fast serving
const downloadCache = new Map();

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

      try {
        const T = (label) => { const t = Date.now(); return [t, label]; };
        let marks = [];
        marks.push(T("start"));
        await prisma.master.update({
          where: { id: masterId },
          data: { status: "PROCESSING" },
        });

        // ── Get source (local temp file preferred, fall back to R2) ───
        let srcBuf = null;
        let sourceDownloadUrl = null;
        let sourceR2Promise = null;

        const sourcePath = job.data.sourcePath;
        if (sourcePath && fs.existsSync(sourcePath)) {
          srcBuf = fs.readFileSync(sourcePath);
          console.log("[QUICK MASTER] Source read from local temp file:", sourcePath);
          marks.push(T("local_src"));

          // Upload source to R2 in background for persistence (crash recovery)
          sourceR2Promise = uploadBuffer({
            key: master.sourceUrl,
            body: srcBuf,
            contentType: master.sourceMime || "audio/mpeg",
          }).then((url) => {
            console.log("[QUICK MASTER] Source uploaded to R2:", url);
            return prisma.master.update({
              where: { id: masterId },
              data: { sourceUrl: url },
            });
          }).catch((err) => {
            console.warn("[QUICK MASTER] Source R2 upload failed (non-fatal):", err.message);
          });
        } else {
          // Fall back to R2 signed URL (handles retries after crash / no local file)
          sourceDownloadUrl = await getDownloadUrl(master.sourceUrl);
          marks.push(T("signed_url"));
        }

        if (srcBuf && srcBuf.byteLength > 150 * 1024 * 1024)
          throw new Error("File too large. Maximum size is 150MB");

        // ── Download artwork from R2 if present ─────────────────────────
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

        // ── Send to Python Mastering Engine ────────────────────
        const formData = new FormData();
        if (srcBuf) {
          formData.append("file", new Blob([srcBuf], { type: master.sourceMime }), master.sourceName);
          srcBuf = null;
        } else {
          formData.append("source_url", sourceDownloadUrl);
          formData.append("filename", master.sourceName);
        }
        formData.append("genre", master.genre);
        marks.push(T("get_src"));
        const meta = master.metadata;
        if (meta) {
          const metaStr = typeof meta === "string" ? meta : JSON.stringify(meta);
          formData.append("metadata", metaStr);
        }
        if (artBuf) {
          const artUrl = master.metadata?.artworkUrl || "";
          const artMime = artUrl.endsWith(".png") ? "image/png" : "image/jpeg";
          formData.append("artwork", new Blob([artBuf], { type: artMime }), "cover." + (artMime === "image/png" ? "png" : "jpg"));
        }

        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 600000);
        let pythonResponse;
        try {
          pythonResponse = await fetch(`${env.PYTHON_ENGINE_URL}/master`, {
            method: "POST", body: formData, signal: controller.signal,
          });
        } catch (fetchError) {
          clearTimeout(timeoutId);
          throw new Error(fetchError.name === "AbortError"
            ? "Python engine request timed out after 10 minutes"
            : `Cannot connect to Python engine: ${fetchError.message}`);
        }
        clearTimeout(timeoutId);
        marks.push(T("python_done"));

        if (!pythonResponse.ok) {
          const errorText = await pythonResponse.text();
          throw new Error(`Python Engine Error: ${pythonResponse.statusText} - ${errorText}`);
        }

        // Read loudness from response headers (available immediately)
        const lufs = parseFloat(pythonResponse.headers.get("X-Lufs-Actual")) || -14;
        const dbtp = parseFloat(pythonResponse.headers.get("X-Tp-Actual")) || -1;
        const dr = parseFloat(pythonResponse.headers.get("X-DR-Actual")) || 6;
        const duration = parseFloat(pythonResponse.headers.get("X-Duration-Actual")) || 0;
        const pyTime = pythonResponse.headers.get("X-Processing-Time-Ms");

        // ── Write mastered audio to temp file + tee to R2 ──────
        const outputKey = `masters/${master.userId}/${Date.now()}-mastered-${master.sourceName}`;
        const tmpPath = path.join(TMP_DIR, `${masterId}.wav`);
        const outputLength = parseInt(pythonResponse.headers.get("content-length"), 10) || undefined;

        // Write local file (fast) while also uploading to R2 in background
        const webStream = pythonResponse.body;
        const nodeStream = Readable.fromWeb(webStream);

        // Split: write to file + upload to R2 simultaneously
        const fileStream = fs.createWriteStream(tmpPath);
        nodeStream.pipe(fileStream);

        // Read the stream for R2 upload (tee: read from temp file after it's written)
        await new Promise((resolve, reject) => {
          fileStream.on("finish", resolve);
          fileStream.on("error", reject);
        });
        marks.push(T("write_local"));

        // Store in download cache for instant serving
        downloadCache.set(masterId, tmpPath);

        // Mark complete immediately — user can download NOW
        const completedAt = new Date();
        const masterMeta =
          master.metadata && typeof master.metadata === "object"
            ? master.metadata
            : {};
        const startedAt = masterMeta.processingStartedAt
          ? new Date(masterMeta.processingStartedAt)
          : null;
        const processingTimeMs = startedAt
          ? completedAt.getTime() - startedAt.getTime()
          : null;

        await prisma.master.update({
          where: { id: masterId },
          data: {
            status: "COMPLETE",
            completedAt,
            lufs,
            dbtp,
            dr,
            duration,
            metadata: {
              ...masterMeta,
              processingTimeMs,
              engineTimeMs: parseInt(pyTime || "0", 10) || null,
            },
          },
        });
        marks.push(T("db_update"));

        // ── Upload output to R2 in background (source already uploading) ──
        const r2UploadPromise = (async () => {
          const fileBuf = fs.readFileSync(tmpPath);
          const outputUrl = await uploadBuffer({
            key: outputKey,
            body: fileBuf,
            contentType: "audio/wav",
          });

          // Wait for source R2 upload to finish too
          await sourceR2Promise;

          await prisma.master.update({
            where: { id: masterId },
            data: { outputUrl },
          });

          // Clean up source temp file
          if (sourcePath && fs.existsSync(sourcePath)) {
            fs.unlink(sourcePath, () => {});
          }

          // Keep output temp file for 5 min for fast downloads, then clean up
          setTimeout(() => {
            downloadCache.delete(masterId);
            fs.unlink(tmpPath, () => {});
          }, 300000);
        })();

        // Notify user
        if (master.user?.email) {
          await sendEmail({
            to: master.user.email,
            subject: "Your Stemy master is ready",
            html: `<p>Your mastered track <strong>${master.sourceName}</strong> is ready to download from your dashboard.</p>`,
          });
        }

        // ── Timing summary ─────────────────────────────────────
        const fmt = (a, b) => `${((b[0] - a[0]) / 1000).toFixed(1)}s`;
        const srcMB = master.sourceSize
          ? (master.sourceSize / 1024 / 1024).toFixed(1)
          : "?";
        const outMB = outputLength ? (outputLength / 1024 / 1024).toFixed(1) : "?";
        const m1 = marks[1];
        const m2 = marks[2] || marks[1];
        const m3 = marks[3] || marks[2];
        const m4 = marks[4] || marks[3];
        console.log(`\n═══ MASTER TIMINGS ═══`);
        console.log(`  Get source     ${fmt(marks[0], m1)}  (${srcMB} MB)`);
        console.log(`  Python engine  ${fmt(m1, m2)}  (py=${(parseInt(pyTime||0)/1000).toFixed(1)}s)`);
        console.log(`  Write local    ${fmt(m2, m3)}  (${outMB} MB)`);
        console.log(`  DB update      ${fmt(m3, m4)}`);
        console.log(`  ─────────────────────────────`);
        console.log(`  USER READY     ${fmt(marks[0], m4)}${processingTimeMs != null ? `  (total=${(processingTimeMs / 1000).toFixed(1)}s)` : ""}`);
        console.log(`  ─────────────────────────────`);
        console.log(`  R2 upload runs in background`);
        console.log(`═══════════════════════════════\n`);
      } catch (error) {
        console.error(`Mastering Job Failed for ${masterId}:`, error);
        throw error;
      }
    },
    { connection: redisConnection, drainDelay: 200, concurrency: 1 },
  );

  worker.on("failed", async (job, error) => {
    if (!job) return;
    await prisma.master.update({
      where: { id: job.data.masterId },
      data: { status: "FAILED", error: error.message },
    });
  });
}

export const enqueueMasteringJob = async (masterId, sourcePath) => {
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

  await masteringQueue.add("process", { masterId, sourcePath });
};

// Export for download endpoint to serve local files
export const getLocalDownloadPath = (masterId) => downloadCache.get(masterId);
