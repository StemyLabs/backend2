import { spawn } from "child_process";
import crypto from "crypto";
import fs from "fs";
import fsp from "fs/promises";
import path from "path";
import ffmpegPath from "ffmpeg-static";
import NodeID3 from "node-id3";
import { getMasterOutputPath, MASTER_TMP_DIR } from "../utils/master-temp.js";
import { getDownloadUrl } from "./storage.service.js";
import { getLocalDownloadPath } from "./queue.service.js";

const mp3Cache = new Map();
const METADATA_SKIP_KEYS = new Set(["artworkUrl", "progress", "progressLabel"]);

const metadataHash = (metadata) => {
  if (!metadata || typeof metadata !== "object") return "";
  const cleaned = {};
  for (const [key, value] of Object.entries(metadata)) {
    if (METADATA_SKIP_KEYS.has(key)) continue;
    if (value == null || String(value).trim() === "") continue;
    cleaned[key] = String(value).trim();
  }
  return crypto
    .createHash("sha256")
    .update(JSON.stringify(cleaned, Object.keys(cleaned).sort()))
    .digest("hex")
    .slice(0, 16);
};

const sidecarPath = (masterId) => path.join(MASTER_TMP_DIR, `${masterId}.mp3.meta`);

const readSidecarHash = async (masterId) => {
  try {
    return (await fsp.readFile(sidecarPath(masterId), "utf8")).trim();
  } catch {
    return null;
  }
};

const writeSidecarHash = async (masterId, hash) => {
  if (!hash) return;
  await fsp.writeFile(sidecarPath(masterId), hash, "utf8");
};

const guessImageMime = (buffer) => {
  if (!buffer || buffer.length < 4) return "image/jpeg";
  if (buffer[0] === 0x89 && buffer[1] === 0x50) return "image/png";
  if (buffer[0] === 0xff && buffer[1] === 0xd8) return "image/jpeg";
  if (buffer[0] === 0x47 && buffer[1] === 0x49) return "image/gif";
  if (buffer[0] === 0x52 && buffer[1] === 0x49) return "image/webp";
  return "image/jpeg";
};

const fetchArtworkBuffer = async (metadata) => {
  const artUrl = metadata?.artworkUrl;
  if (!artUrl) return null;
  try {
    const signedUrl = await getDownloadUrl(artUrl);
    const resp = await fetch(signedUrl);
    if (!resp.ok) return null;
    return Buffer.from(await resp.arrayBuffer());
  } catch (err) {
    console.warn("[DOWNLOAD] Failed to fetch artwork for MP3 tags:", err.message);
    return null;
  }
};

/** Write ID3v2 tags onto an MP3 from master metadata (incl. ISRC, composer, copyright). */
export const embedMp3Metadata = async (mp3Path, metadata, artworkBuffer = null) => {
  const meta = metadata && typeof metadata === "object" ? metadata : null;
  if (!meta) return;

  const tags = {};
  const set = (key, val) => {
    if (val != null && String(val).trim()) tags[key] = String(val).trim();
  };

  set("title", meta.title);
  set("artist", meta.artist);
  set("album", meta.album);
  set("year", meta.year);
  set("trackNumber", meta.track);
  set("genre", meta.genre);
  set("composer", meta.composer);
  set("copyright", meta.copyright);
  if (meta.isrc) tags.ISRC = String(meta.isrc).trim();
  if (meta.comment) set("comment", { language: "eng", text: String(meta.comment) });

  let artBuf = artworkBuffer;
  if (!artBuf && meta.artworkUrl) {
    artBuf = await fetchArtworkBuffer(meta);
  }
  if (artBuf?.length) {
    tags.image = {
      mime: guessImageMime(artBuf),
      type: { id: 3, name: "front cover" },
      description: "Cover",
      imageBuffer: artBuf,
    };
  }

  if (Object.keys(tags).length === 0) return;

  const ok = NodeID3.update(tags, mp3Path);
  if (!ok) {
    throw new Error("Failed to embed ID3 metadata into MP3");
  }
};

/** Resolve local mastered WAV if it exists on disk. */
export const resolveLocalWavPath = (master) => {
  const cached = getLocalDownloadPath(master.id);
  if (cached && fs.existsSync(cached)) return cached;

  const standardPath = getMasterOutputPath(master.id);
  if (fs.existsSync(standardPath)) return standardPath;

  if (master.outputUrl?.startsWith("local://")) {
    const key = master.outputUrl.replace("local://", "");
    const diskPath = path.join(MASTER_TMP_DIR, path.basename(key));
    if (fs.existsSync(diskPath)) return diskPath;
  }

  return null;
};

/** Ensure WAV exists locally — download from R2 when needed (for MP3 conversion). */
export const ensureWavOnDisk = async (master) => {
  const existing = resolveLocalWavPath(master);
  if (existing) return existing;

  if (!master.outputUrl || master.outputUrl.startsWith("local://")) {
    return null;
  }

  const dest = getMasterOutputPath(master.id);
  const signedUrl = await getDownloadUrl(master.outputUrl);
  const resp = await fetch(signedUrl);
  if (!resp.ok) {
    throw new Error(`Failed to fetch master WAV from storage (${resp.status})`);
  }
  await fsp.writeFile(dest, Buffer.from(await resp.arrayBuffer()));
  console.log("[DOWNLOAD] Cached WAV from storage for", master.id);
  return dest;
};

const runFfmpeg = (args) =>
  new Promise((resolve, reject) => {
    if (!ffmpegPath) {
      reject(new Error("FFmpeg binary not available"));
      return;
    }
    const proc = spawn(ffmpegPath, args, { windowsHide: true });
    let stderr = "";
    proc.stderr.on("data", (chunk) => {
      stderr += chunk.toString();
    });
    proc.on("error", reject);
    proc.on("close", (code) => {
      if (code === 0) resolve();
      else reject(new Error(stderr.trim() || `FFmpeg exited with code ${code}`));
    });
  });

const isMp3CacheValid = async (masterId, wavPath, metaHash) => {
  const outPath = path.join(MASTER_TMP_DIR, `${masterId}.mp3`);
  if (!fs.existsSync(outPath)) return false;

  const wavStat = await fsp.stat(wavPath);
  const mp3Stat = await fsp.stat(outPath);
  if (mp3Stat.mtimeMs < wavStat.mtimeMs) return false;

  const storedHash = await readSidecarHash(masterId);
  if (metaHash && storedHash !== metaHash) return false;

  return true;
};

/** Convert mastered WAV to MP3 and embed full ID3 metadata from master record. */
export const getMasterMp3Path = async (masterId, wavPath, metadata = null) => {
  const metaHash = metadataHash(metadata);

  const cached = mp3Cache.get(masterId);
  if (cached && fs.existsSync(cached) && (await isMp3CacheValid(masterId, wavPath, metaHash))) {
    return cached;
  }

  const outPath = path.join(MASTER_TMP_DIR, `${masterId}.mp3`);
  if (await isMp3CacheValid(masterId, wavPath, metaHash)) {
    mp3Cache.set(masterId, outPath);
    return outPath;
  }

  await runFfmpeg([
    "-y",
    "-i",
    wavPath,
    "-codec:a",
    "libmp3lame",
    "-b:a",
    "320k",
    "-map_metadata",
    "0",
    "-id3v2_version",
    "3",
    outPath,
  ]);

  await embedMp3Metadata(outPath, metadata);
  await writeSidecarHash(masterId, metaHash);

  mp3Cache.set(masterId, outPath);
  return outPath;
};

export const getMasterMp3PathIfExists = async (masterId, wavPath, metadata = null) => {
  const metaHash = metadataHash(metadata);
  const cached = mp3Cache.get(masterId);
  if (cached && fs.existsSync(cached)) {
    if (!wavPath || (await isMp3CacheValid(masterId, wavPath, metaHash))) {
      return cached;
    }
  }

  const outPath = path.join(MASTER_TMP_DIR, `${masterId}.mp3`);
  if (!fs.existsSync(outPath)) return null;
  if (wavPath && !(await isMp3CacheValid(masterId, wavPath, metaHash))) return null;

  mp3Cache.set(masterId, outPath);
  return outPath;
};
