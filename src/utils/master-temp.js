import fs from "fs";
import os from "os";
import path from "path";

/** Shared temp dir for multer uploads and BullMQ mastering jobs. */
export const MASTER_TMP_DIR = path.join(os.tmpdir(), "stemy-masters");

export const ensureMasterTmpDir = () => {
  try {
    if (!fs.existsSync(MASTER_TMP_DIR)) {
      fs.mkdirSync(MASTER_TMP_DIR, { recursive: true });
    }
  } catch (e) {
    console.error("Failed to create MASTER_TMP_DIR:", e);
  }
  return MASTER_TMP_DIR;
};

// Initial creation
ensureMasterTmpDir();

/** Standard on-disk path for a completed master WAV (survives in-memory cache loss). */
export const getMasterOutputPath = (masterId) => {
  ensureMasterTmpDir();
  return path.join(MASTER_TMP_DIR, `${masterId}.wav`);
};
