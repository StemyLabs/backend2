import fs from "fs";
import fsp from "fs/promises";
import os from "os";
import path from "path";

/** Read at call time so dotenv / systemd env is applied before first use. */
export const getMasterTmpDir = () => {
  const dir =
    process.env.STEMY_TEMP_DIR || path.join(os.tmpdir(), "stemy-masters");
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
  return dir;
};

/** @deprecated use getMasterTmpDir() — kept for imports that expect a string */
export const MASTER_TMP_DIR = getMasterTmpDir();

/** Canonical on-disk source path for a mastering job. */
export const masterSourcePath = (masterId, sourceNameOrExt) => {
  const ext = sourceNameOrExt?.startsWith(".")
    ? sourceNameOrExt
    : path.extname(sourceNameOrExt || "") || ".audio";
  return path.join(getMasterTmpDir(), `${masterId}-source${ext}`);
};

/**
 * Move/copy multer upload to {masterId}-source{ext}. Returns dest path or null.
 */
export const stageMasterSource = async (masterId, uploadPath, sourceName) => {
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

/** Find staged source when extension in DB does not match disk. */
export const findStagedMasterSource = async (masterId) => {
  const dir = getMasterTmpDir();
  const prefix = `${masterId}-source`;
  const entries = await fsp.readdir(dir);
  const match = entries.find((name) => name.startsWith(prefix));
  if (!match) return null;
  const full = path.join(dir, match);
  return fs.existsSync(full) ? full : null;
};
