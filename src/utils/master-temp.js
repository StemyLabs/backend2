import fs from "fs";
import os from "os";
import path from "path";

/** Shared temp dir for multer uploads, BullMQ jobs, and Python STEMY_TEMP_DIR on VPS. */
export const MASTER_TMP_DIR =
  process.env.STEMY_TEMP_DIR || path.join(os.tmpdir(), "stemy-masters");

if (!fs.existsSync(MASTER_TMP_DIR)) {
  fs.mkdirSync(MASTER_TMP_DIR, { recursive: true });
}
