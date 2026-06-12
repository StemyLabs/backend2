import { spawn } from "child_process";
import fs from "fs";
import fsp from "fs/promises";
import path from "path";
import ffmpegPath from "ffmpeg-static";
import { getMasterOutputPath, MASTER_TMP_DIR } from "../utils/master-temp.js";
import { getDownloadUrl } from "./storage.service.js";
import { getLocalDownloadPath } from "./queue.service.js";

const mp3Cache = new Map();

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

/** Convert mastered WAV to MP3; preserves embedded ID3 tags (title, artist, artwork). */
export const getMasterMp3Path = async (masterId, wavPath) => {
  const cached = mp3Cache.get(masterId);
  if (cached && fs.existsSync(cached)) return cached;

  const outPath = path.join(MASTER_TMP_DIR, `${masterId}.mp3`);
  if (fs.existsSync(outPath)) {
    const wavStat = await fsp.stat(wavPath);
    const mp3Stat = await fsp.stat(outPath);
    if (mp3Stat.mtimeMs >= wavStat.mtimeMs) {
      mp3Cache.set(masterId, outPath);
      return outPath;
    }
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

  mp3Cache.set(masterId, outPath);
  return outPath;
};

export const getMasterMp3PathIfExists = (masterId) => {
  const cached = mp3Cache.get(masterId);
  if (cached && fs.existsSync(cached)) return cached;
  const outPath = path.join(MASTER_TMP_DIR, `${masterId}.mp3`);
  if (fs.existsSync(outPath)) return outPath;
  return null;
};
