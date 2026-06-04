import fs from "fs";
import fsp from "fs/promises";
import os from "os";
import path from "path";

/** Shared temp dir for multer uploads, BullMQ jobs, and Python STEMY_TEMP_DIR on VPS. */
export const MASTER_TMP_DIR =
  process.env.STEMY_TEMP_DIR || path.join(os.tmpdir(), "stemy-masters");

export const ensureMasterTmpDir = () => {
  if (!fs.existsSync(MASTER_TMP_DIR)) {
    fs.mkdirSync(MASTER_TMP_DIR, { recursive: true });
  }
  return MASTER_TMP_DIR;
};

ensureMasterTmpDir();

/** Canonical on-disk source path for a mastering job (do not rely on in-memory maps). */
export const masterSourcePath = (masterId, sourceNameOrExt) => {
  const ext = sourceNameOrExt?.startsWith(".")
    ? sourceNameOrExt
    : path.extname(sourceNameOrExt || "") || ".audio";
  return path.join(MASTER_TMP_DIR, `${masterId}-source${ext}`);
};

/**
 * Move/copy multer upload to {masterId}-source{ext}. Returns dest path or null.
 */
export const stageMasterSource = async (masterId, uploadPath, sourceName) => {
  ensureMasterTmpDir();
  if (!uploadPath || !fs.existsSync(uploadPath)) {
    return null;
  }
  const ext =
    path.extname(uploadPath) || path.extname(sourceName || "") || ".audio";
  const dest = masterSourcePath(masterId, ext);
  try {
    await fsp.rename(uploadPath, dest);
  } catch {
    await fsp.copyFile(uploadPath, dest);
    await fsp.unlink(uploadPath).catch(() => {});
  }
  if (!fs.existsSync(dest)) {
    return null;
  }
  const stat = await fsp.stat(dest);
  if (stat.size < 1) {
    await fsp.unlink(dest).catch(() => {});
    return null;
  }
  return dest;
};

/** Find staged source when extension in DB does not match disk (e.g. .mp3 vs .wav). */
export const findStagedMasterSource = async (masterId) => {
  ensureMasterTmpDir();
  const prefix = `${masterId}-source`;
  const entries = await fsp.readdir(MASTER_TMP_DIR);
  const match = entries.find((name) => name.startsWith(prefix));
  if (!match) return null;
  const full = path.join(MASTER_TMP_DIR, match);
  return fs.existsSync(full) ? full : null;
};
