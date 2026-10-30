import express from "express";
import { authMiddleware } from "../middleware/auth.middleware.js";
import {
  recordMixCheck,
  getMixCheckHistory,
} from "../controllers/mixcheck.controller.js";

const router = express.Router();

router.post("/", authMiddleware, recordMixCheck);
router.get("/", authMiddleware, getMixCheckHistory);

export default router;
