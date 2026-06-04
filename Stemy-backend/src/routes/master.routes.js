import express from "express";
import multer from "multer";
import fs from "fs";
import os from "os";
import path from "path";
import { authMiddleware } from "../middleware/auth.middleware.js";
import {
  createQuickMaster,
  listMasters,
  getMasterById,
  getMasterDownload,
} from "../controllers/master.controller.js";

const UPLOAD_DIR = path.join(os.tmpdir(), "stemy-uploads");
if (!fs.existsSync(UPLOAD_DIR)) fs.mkdirSync(UPLOAD_DIR, { recursive: true });

const router = express.Router();
const upload = multer({
  storage: multer.diskStorage({
    destination: (_req, _file, cb) => cb(null, UPLOAD_DIR),
    filename: (_req, file, cb) => {
      const safe = file.originalname.replace(/[^\w.\-]+/g, "_");
      cb(null, `${Date.now()}-${safe}`);
    },
  }),
});

router.post("/quick", authMiddleware, upload.fields([
  { name: "audio", maxCount: 1 },
  { name: "artwork", maxCount: 1 },
]), createQuickMaster);
router.get("/", authMiddleware, listMasters);
router.get("/:id", authMiddleware, getMasterById);
router.get("/:id/download", authMiddleware, getMasterDownload);

export default router;
