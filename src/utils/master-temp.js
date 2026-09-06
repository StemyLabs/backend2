import fs from "fs";
import os from "os";
import path from "path";

/** Shared temp dir for multer uploads and BullMQ mastering jobs. */
export const MASTER_TMP_DIR = path.join(os.tmpdir(), "stemy-masters");

/** Standard on-disk path for a completed master WAV (survives in-memory cache loss). */
export const getMasterOutputPath = (masterId) =>
  path.join(MASTER_TMP_DIR, `${masterId}.wav`);

if (!fs.existsSync(MASTER_TMP_DIR)) {
  fs.mkdirSync(MASTER_TMP_DIR, { recursive: true });
}
