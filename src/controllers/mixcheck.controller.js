import { prisma } from "../lib/prisma.js";

// 1. Record a new Mix Check version
export const recordMixCheck = async (req, res) => {
  try {
    const { songTitle, versionNo, score, verdict, metrics, note, masterId } = req.body;

    const user = await prisma.user.findUnique({
      where: { id: req.userId },
      select: { id: true, plan: true },
    });

    if (!user) {
      return res.status(404).json({ message: "User not found" });
    }

    // Basic plan: 1 free check quota enforcement
    const isProOrPlus = user.plan === "PRO";

    // Count user's total past mix checks
    const userMasters = await prisma.master.findMany({
      where: { userId: req.userId },
      select: { id: true, metadata: true },
    });

    let totalChecks = 0;
    userMasters.forEach((m) => {
      if (m.metadata && Array.isArray(m.metadata.mixChecks)) {
        totalChecks += m.metadata.mixChecks.length;
      }
    });

    if (!isProOrPlus && totalChecks >= 1) {
      return res.status(403).json({
        upgradeRequired: true,
        message: "Basic plan includes 1 free Mix Check. Upgrade to Plus for unlimited checks on every song.",
      });
    }

    const versionItem = {
      versionNo: versionNo || 1,
      date: new Date().toISOString(),
      score: Math.max(0, Math.min(100, Math.round(score || 0))),
      verdict: verdict || (score >= 80 ? "READY" : score >= 50 ? "CHECK" : "FIX"),
      metrics: metrics || {},
      note: note || "",
    };

    // If masterId provided, attach to that master
    let targetMaster = null;
    if (masterId) {
      targetMaster = userMasters.find((m) => m.id === masterId);
    }
    if (!targetMaster && userMasters.length > 0) {
      targetMaster = userMasters[0];
    }

    if (targetMaster) {
      const currentMeta = (typeof targetMaster.metadata === "object" && targetMaster.metadata !== null)
        ? targetMaster.metadata
        : {};
      const currentChecks = Array.isArray(currentMeta.mixChecks) ? currentMeta.mixChecks : [];
      versionItem.versionNo = currentChecks.length + 1;
      const updatedChecks = [...currentChecks, versionItem];

      await prisma.master.update({
        where: { id: targetMaster.id },
        data: {
          metadata: {
            ...currentMeta,
            mixChecks: updatedChecks,
            latestMixScore: versionItem.score,
          },
        },
      });
    }

    return res.json({
      message: "Mix check recorded successfully",
      version: versionItem,
      totalChecks: totalChecks + 1,
      isUnlimited: isProOrPlus,
    });
  } catch (err) {
    console.error("[MIXCHECK] record error:", err);
    return res.status(500).json({ message: "Failed to record mix check" });
  }
};

// 2. Get Mix Check History for the current user
export const getMixCheckHistory = async (req, res) => {
  try {
    const user = await prisma.user.findUnique({
      where: { id: req.userId },
      select: { id: true, plan: true },
    });

    if (!user) {
      return res.status(404).json({ message: "User not found" });
    }

    const masters = await prisma.master.findMany({
      where: { userId: req.userId },
      orderBy: { createdAt: "desc" },
      select: {
        id: true,
        sourceName: true,
        metadata: true,
        createdAt: true,
      },
    });

    const songs = [];
    let totalChecksCount = 0;

    masters.forEach((m) => {
      const meta = m.metadata || {};
      const checks = Array.isArray(meta.mixChecks) ? meta.mixChecks : [];
      if (checks.length > 0) {
        totalChecksCount += checks.length;
        songs.push({
          masterId: m.id,
          title: meta.title || m.sourceName || "Untitled Track",
          artist: meta.artist || "Artist",
          createdAt: m.createdAt,
          versions: checks,
          latestScore: checks[checks.length - 1].score,
          latestVerdict: checks[checks.length - 1].verdict,
          sharePage: meta.sharePage || null,
        });
      }
    });

    return res.json({
      songs,
      totalChecks: totalChecksCount,
      plan: user.plan,
      isUnlimited: user.plan === "PRO",
    });
  } catch (err) {
    console.error("[MIXCHECK] get history error:", err);
    return res.status(500).json({ message: "Failed to load mix check history" });
  }
};

