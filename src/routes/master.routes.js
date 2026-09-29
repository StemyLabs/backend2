import express from "express";
import multer from "multer";
import { authMiddleware } from "../middleware/auth.middleware.js";
import { MASTER_TMP_DIR } from "../utils/master-temp.js";
import {
  createQuickMaster,
  listMasters,
  getMasterById,
  getMasterDownload,
} from "../controllers/master.controller.js";

const router = express.Router();
const upload = multer({
  storage: multer.diskStorage({
    destination: (_req, _file, cb) => cb(null, MASTER_TMP_DIR),
    filename: (_req, file, cb) => {
      const safe = (file.originalname || "upload.audio").replace(/[^\w.\-]+/g, "_");
      cb(null, `${Date.now()}-${safe}`);
    },
  }),
  limits: { fileSize: 110 * 1024 * 1024 },
});

const uploadFields = upload.fields([
  { name: "audio", maxCount: 1 },
  { name: "artwork", maxCount: 1 },
]);

const handleUpload = (req, res, next) => {
  uploadFields(req, res, (err) => {
    if (err) {
      console.error("[QUICK MASTER] Multer upload error:", err);
      return res.status(400).json({
        message: err instanceof multer.MulterError
          ? `File upload error: ${err.message}`
          : (err.message || "File upload failed"),
      });
    }
    next();
  });
};

router.post("/quick", authMiddleware, handleUpload, createQuickMaster);
router.get("/", authMiddleware, listMasters);
router.get("/:id", authMiddleware, getMasterById);
router.get("/:id/download", authMiddleware, getMasterDownload);

export default router;
