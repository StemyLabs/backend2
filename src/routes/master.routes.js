import express from "express";
import multer from "multer";
import { authMiddleware } from "../middleware/auth.middleware.js";
import { getMasterTmpDir } from "../utils/master-temp.js";
import {
  createQuickMaster,
  listMasters,
  getMasterById,
  getMasterDownload,
} from "../controllers/master.controller.js";

const router = express.Router();
const upload = multer({
  storage: multer.diskStorage({
    destination: (_req, _file, cb) => cb(null, getMasterTmpDir()),
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
