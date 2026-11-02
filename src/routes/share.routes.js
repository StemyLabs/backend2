import express from "express";
import { authMiddleware } from "../middleware/auth.middleware.js";
import {
  createOrUpdateSharePage,
  getSharePage,
  updateSharePageSettings,
  deleteSharePage,
  voteSharePage,
  incrementShareCount,
  incrementPlayCount,
  downloadShareMp3,
  streamShareAudio,
  getMasterDetailsForShare,
} from "../controllers/share.controller.js";

const router = express.Router();

// Public master details for share builder
router.get("/master-details/:masterId", getMasterDetailsForShare);

// Owner actions (Authenticated)
router.post("/", authMiddleware, createOrUpdateSharePage);
router.patch("/:id", authMiddleware, updateSharePageSettings);
router.delete("/:id", authMiddleware, deleteSharePage);

// Audio stream & download
router.get("/:id/stream", streamShareAudio);
router.get("/:id/audio", streamShareAudio);
router.get("/:id/download", downloadShareMp3);

// Public listener actions
router.get("/:slugOrId", getSharePage);
router.post("/:id/vote", voteSharePage);
router.post("/:id/share", incrementShareCount);
router.post("/:id/play", incrementPlayCount);

export default router;
