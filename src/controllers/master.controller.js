import { env } from "../config/env.js";
import { prisma } from "../lib/prisma.js";
import { uploadBuffer, uploadStream, getDownloadUrl } from "../services/storage.service.js";
import { enqueueMasteringJob } from "../services/queue.service.js";
import {
  getMasterMp3Path,
  getMasterMp3PathIfExists,
  ensureWavOnDisk,
  resolveLocalWavPath,
} from "../services/audio-export.service.js";
import { PURGED_URL } from "../services/retention.service.js";
import { MASTER_TMP_DIR } from "../utils/master-temp.js";
import https from "https";
import http from "http";
import fs from "fs";
import fsp from "fs/promises";
import path from "path";

const ALLOWED_PLANS = ["BASIC", "PRO"];

const checkUserPlan = async (req) => {
  try {
    if (req.user?.plan && ALLOWED_PLANS.includes(req.user.plan)) {
      return true;
    }
    const subscription = await prisma.subscription.findFirst({
      where: { userId: req.userId },
      orderBy: { createdAt: "desc" },
    });
    if (!subscription) return true; // Default allow for basic registered users
    const isActive = ["ACTIVE", "TRIALING"].includes(subscription.status);
    return isActive && ALLOWED_PLANS.includes(subscription.plan);
  } catch (err) {
    console.warn("[CHECK PLAN] Subscription lookup warning:", err.message);
    return true; // Fallback to allowing access
  }
};

const ALLOWED_MIME = new Set([
  "audio/wav",
  "audio/x-wav",
  "audio/wave",
  "audio/vnd.wave",
  "audio/mpeg",
  "audio/mp3",
  "audio/x-mp3",
  "audio/flac",
  "audio/x-flac",
  "audio/aiff",
  "audio/x-aiff",
  "audio/mp4",
  "audio/x-m4a",
  "audio/aac",
  "audio/ogg",
  "audio/webm",
  "application/octet-stream",
]);

const streamFileDownload = (res, filePath, sourceName, { contentType, ext }) => {
  const base = sourceName?.replace(/\.[^.]+$/, "") || "track";
  const filename = `mastered-${base}.${ext}`;
  const stat = fs.statSync(filePath);
  res.setHeader("Content-Type", contentType);
  res.setHeader("Content-Disposition", `attachment; filename="${filename}"`);
  res.setHeader("Content-Length", String(stat.size));
  res.setHeader("Cache-Control", "no-store");
  const stream = fs.createReadStream(filePath);
  stream.on("error", (err) => {
    console.error("[DOWNLOAD] Stream error:", err.message);
    if (!res.headersSent) res.status(500).json({ message: "Failed to read file" });
    else res.destroy();
  });
  stream.pipe(res);
};

const streamWavDownload = (res, filePath, sourceName) =>
  streamFileDownload(res, filePath, sourceName, {
    contentType: "audio/wav",
    ext: "wav",
  });

const streamMp3Download = (res, filePath, sourceName) =>
  streamFileDownload(res, filePath, sourceName, {
    contentType: "audio/mpeg",
    ext: "mp3",
  });

export const createQuickMaster = async (req, res) => {
  try {
    const hasValidPlan = await checkUserPlan(req);
    if (!hasValidPlan) {
      return res.status(403).json({ 
        message: "Quick Master requires a Basic or Pro subscription" 
      });
    }

    console.log("[QUICK MASTER] New Quick Master request received");
    console.log("[QUICK MASTER] Request user ID:", req.userId);
    console.log("[QUICK MASTER] Request body keys:", Object.keys(req.body));
    console.log(
      "[QUICK MASTER] Request file info:",
      req.file
        ? {
            originalname: req.file.originalname,
            mimetype: req.file.mimetype,
            size: req.file.size,
          }
        : "No file",
    );

    const file = req.files?.audio?.[0] || req.file;
    const { genre, metadata: metadataRaw } = req.body;
    const artwork = req.files?.artwork?.[0] || null;

    if (!file) {
      console.error("[QUICK MASTER] No file provided");
      return res.status(400).json({ message: "Audio file is required" });
    }
    if (!genre) {
      console.error("[QUICK MASTER] No genre provided");
      return res.status(400).json({ message: "Genre is required" });
    }
    if (file.size > 100 * 1024 * 1024) {
      console.error("[QUICK MASTER] File too large:", file.size, "bytes");
      return res.status(400).json({ message: "File exceeds 100MB limit" });
    }
    if (file.mimetype && !ALLOWED_MIME.has(file.mimetype)) {
      console.error("[QUICK MASTER] Unsupported MIME type:", file.mimetype);
      return res.status(400).json({ message: "Unsupported audio format" });
    }

    // Parse metadata safely
    let parsedMetadata = null;
    if (metadataRaw) {
      try {
        parsedMetadata = typeof metadataRaw === "string" ? JSON.parse(metadataRaw) : metadataRaw;
      } catch (jsonErr) {
        console.warn("[QUICK MASTER] Invalid metadata JSON received:", jsonErr.message);
        parsedMetadata = {};
      }
    }

    let artworkCachePath = null;

    // Upload / cache artwork if provided
    if (artwork) {
      const artExt =
        path.extname(artwork.originalname || "") ||
        (artwork.mimetype === "image/png" ? ".png" : ".jpg");
      artworkCachePath = path.join(
        MASTER_TMP_DIR,
        `pending-art-${Date.now()}${artExt}`,
      );

      if (artwork.path) {
        await fsp.copyFile(artwork.path, artworkCachePath);
      } else if (artwork.buffer?.length) {
        await fsp.writeFile(artworkCachePath, artwork.buffer);
      } else {
        artworkCachePath = null;
        console.warn("[QUICK MASTER] Artwork file had no readable data");
      }

      let artUrl = null;
      try {
        const artKey = `artwork/${req.userId}/${Date.now()}-${artwork.originalname}`;
        const artType = artwork.mimetype || "image/jpeg";
        if (artwork.path && fs.existsSync(artwork.path)) {
          const artStat = await fsp.stat(artwork.path);
          artUrl = await uploadStream({
            key: artKey,
            stream: fs.createReadStream(artwork.path),
            contentType: artType,
            contentLength: artStat.size,
          });
        } else if (artwork.buffer) {
          artUrl = await uploadBuffer({
            key: artKey,
            body: artwork.buffer,
            contentType: artType,
          });
        }
      } catch (artErr) {
        console.warn("[QUICK MASTER] R2 artwork upload failed (falling back to local):", artErr.message);
        artUrl = artworkCachePath ? `local://${path.basename(artworkCachePath)}` : null;
      }

      if (artUrl) {
        parsedMetadata = { ...parsedMetadata, artworkUrl: artUrl };
        console.log("[QUICK MASTER] Artwork url set to:", artUrl);
      }
    }

    let audioDiskPath = file.path;
    if ((!audioDiskPath || !fs.existsSync(audioDiskPath)) && file.buffer?.length) {
      const safeName = (file.originalname || "audio.wav").replace(/[^\w.\-]+/g, "_");
      audioDiskPath = path.join(MASTER_TMP_DIR, `${Date.now()}-${safeName}`);
      await fsp.writeFile(audioDiskPath, file.buffer);
    }

    let sourceUrl;
    const isVps = env.IS_VPS === "true";

    if (isVps) {
      if (!audioDiskPath || !fs.existsSync(audioDiskPath)) {
        console.error("[QUICK MASTER] VPS upload missing audio file on disk:", audioDiskPath);
        return res.status(400).json({ message: "Audio upload failed — please try again" });
      }
      sourceUrl = `local://pending/${req.userId}`;
      console.log("[QUICK MASTER] VPS mode — source at", audioDiskPath);
    } else {
      const sourceKey = `masters/${req.userId}/${Date.now()}-${file.originalname}`;
      try {
        if (audioDiskPath && fs.existsSync(audioDiskPath)) {
          const srcStat = await fsp.stat(audioDiskPath);
          sourceUrl = await uploadStream({
            key: sourceKey,
            stream: fs.createReadStream(audioDiskPath),
            contentType: file.mimetype || "application/octet-stream",
            contentLength: srcStat.size,
          });
        } else if (file.buffer) {
          sourceUrl = await uploadBuffer({
            key: sourceKey,
            body: file.buffer,
            contentType: file.mimetype || "application/octet-stream",
          });
        }
      } catch (upErr) {
        console.warn("[QUICK MASTER] Source upload to R2 failed (falling back to local disk):", upErr.message);
        sourceUrl = `local://pending/${req.userId}`;
      }
      console.log("[QUICK MASTER] Source URL set to:", sourceUrl);
    }

    let cleanMetadata = null;
    if (parsedMetadata && typeof parsedMetadata === "object") {
      try {
        cleanMetadata = JSON.parse(JSON.stringify(parsedMetadata));
      } catch {
        cleanMetadata = null;
      }
    }

    console.log("[QUICK MASTER] Creating database record...");
    const master = await prisma.master.create({
      data: {
        userId: req.userId,
        genre,
        type: "QUICK",
        sourceName: file.originalname,
        sourceMime: file.mimetype || "application/octet-stream",
        sourceSize: file.size,
        sourceUrl: sourceUrl || `local://pending/${req.userId}`,
        metadata: cleanMetadata,
      },
    });
    console.log("[QUICK MASTER] Database record created with ID:", master.id);

    console.log("[QUICK MASTER] Enqueuing mastering job...");
    await enqueueMasteringJob(
      master.id,
      audioDiskPath || null,
      artworkCachePath,
    );
    console.log(
      "[QUICK MASTER] Master %s returned QUEUED — client polls GET /masters/:id",
      master.id,
    );

    return res.status(201).json({ master });
  } catch (error) {
    console.error("Create quick master error:", error?.stack || error);
    return res
      .status(500)
      .json({ message: error?.message || "Failed to create quick master job" });
  }
};

export const listMasters = async (req, res) => {
  const masters = await prisma.master.findMany({
    where: { userId: req.userId },
    orderBy: { createdAt: "desc" },
  });
  return res.json({ masters });
};

export const getMasterById = async (req, res) => {
  const master = await prisma.master.findFirst({
    where: { id: req.params.id, userId: req.userId },
  });
  if (!master) {
    return res.status(404).json({ message: "Master not found" });
  }
  return res.json({ master });
};

export const getMasterDownload = async (req, res) => {
  const master = await prisma.master.findFirst({
    where: { id: req.params.id, userId: req.userId },
  });
  if (!master) {
    return res.status(404).json({ message: "Master not found" });
  }
  if (master.status !== "COMPLETE") {
    return res.status(409).json({ message: "Master output is not ready" });
  }
  if (
    master.filesPurgedAt ||
    master.outputUrl === PURGED_URL ||
    master.sourceUrl === PURGED_URL
  ) {
    return res.status(410).json({
      message:
        "This file has been removed from storage after your subscription grace period ended.",
    });
  }

  const format = String(req.query.format || "wav").toLowerCase();

  if (format === "mp3") {
    try {
      const wavPath =
        resolveLocalWavPath(master) || (await ensureWavOnDisk(master));
      if (!wavPath) {
        return res.status(409).json({
          message: "Master file is no longer available. Please run a new master.",
        });
      }

      const cachedMp3 = await getMasterMp3PathIfExists(
        master.id,
        wavPath,
        master.metadata,
      );
      if (cachedMp3) {
        return streamMp3Download(res, cachedMp3, master.sourceName);
      }

      const mp3Path = await getMasterMp3Path(master.id, wavPath, master.metadata);
      return streamMp3Download(res, mp3Path, master.sourceName);
    } catch (err) {
      console.error("[DOWNLOAD] MP3 conversion failed:", err.message);
      return res.status(500).json({
        message: "MP3 conversion failed. Try downloading WAV instead.",
      });
    }
  }

  const localPath = resolveLocalWavPath(master);
  if (localPath) {
    return streamWavDownload(res, localPath, master.sourceName);
  }

  // Fall back to R2 (or wait if background upload still running)
  if (!master.outputUrl || master.outputUrl.startsWith("local://")) {
    return res.status(409).json({
      message: "Master file is no longer on the server. Please run a new master.",
    });
  }

  const signedUrl = await getDownloadUrl(master.outputUrl);

  const urlObj = new URL(master.outputUrl);
  const filename = urlObj.pathname.split("/").pop() || "mastered-track.wav";

  res.setHeader("Content-Type", "audio/wav");
  res.setHeader("Content-Disposition", `attachment; filename="${filename}"`);
  res.setHeader("Cache-Control", "no-store");

  const proxyUrl = new URL(signedUrl);
  const protocol = proxyUrl.protocol === "https:" ? https : http;

  const proxyReq = protocol.request(proxyUrl, (proxyRes) => {
    if (proxyRes.statusCode !== 200) {
      res.status(proxyRes.statusCode || 500).json({ message: "Failed to fetch file from storage" });
      return;
    }
    proxyRes.pipe(res);
  });

  proxyReq.on("error", (err) => {
    console.error("[DOWNLOAD] Proxy error:", err.message);
    res.status(500).json({ message: "Failed to download file" });
  });

  proxyReq.end();
};
