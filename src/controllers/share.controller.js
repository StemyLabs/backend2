import fs from "fs";
import path from "path";
import { prisma } from "../lib/prisma.js";
import { MASTER_TMP_DIR } from "../utils/master-temp.js";
import { resolveLocalWavPath } from "../services/audio-export.service.js";

// Helper to create clean URL-safe slugs
function slugify(text) {
  return String(text || "")
    .toLowerCase()
    .trim()
    .replace(/[^\w\s-]/g, "")
    .replace(/[\s_-]+/g, "-")
    .replace(/^-+|-+$/g, "");
}

// 0. Get master details for share page creation (public info)
export const getMasterDetailsForShare = async (req, res) => {
  try {
    const { masterId } = req.params;

    const master = await prisma.master.findUnique({
      where: { id: masterId },
      select: {
        id: true,
        userId: true,
        outputUrl: true,
        sourceUrl: true,
        sourceName: true,
        sourceSize: true,
        duration: true,
        metadata: true,
        status: true,
      },
    });

    if (!master) {
      return res.status(404).json({ message: "Master not found" });
    }

    return res.json({ master });
  } catch (err) {
    console.error("[SHARE] getMasterDetailsForShare error:", err);
    return res.status(500).json({ message: "Failed to load master details" });
  }
};

// 1. Create or update Share Page for a finished master (Owner only)
export const createOrUpdateSharePage = async (req, res) => {
  try {
    const { masterId, artist, title, caption, artworkUrl, allowDownload } = req.body;

    if (!masterId) {
      return res.status(400).json({ message: "masterId is required" });
    }

    const master = await prisma.master.findUnique({
      where: { id: masterId },
    });

    if (!master) {
      return res.status(404).json({ message: "Master not found" });
    }

    if (master.userId !== req.userId) {
      return res.status(403).json({ message: "Access denied" });
    }

    const currentMeta = (typeof master.metadata === "object" && master.metadata !== null)
      ? master.metadata
      : {};

    const existingPage = currentMeta.sharePage || {};

    const safeArtist = (artist || currentMeta.artist || "Artist").trim();
    const safeTitle = (title || currentMeta.title || master.sourceName || "Track").trim();
    const shortId = master.id.slice(0, 8);
    const slug = existingPage.slug || `${slugify(safeArtist)}-${slugify(safeTitle)}-${shortId}`;

    const host = req.headers["x-forwarded-host"] || req.headers.host || "localhost:8000";
    const proto = req.headers["x-forwarded-proto"] || req.protocol || "http";
    const streamFallbackUrl = `${proto}://${host}/api/share-pages/${encodeURIComponent(slug)}/stream`;
    const resolvedAudioUrl = (master.outputUrl && master.outputUrl.startsWith("http"))
      ? master.outputUrl
      : streamFallbackUrl;

    const sharePage = {
      id: master.id,
      masterId: master.id,
      slug,
      artist: safeArtist,
      title: safeTitle,
      caption: (caption !== undefined ? caption : existingPage.caption || "").trim(),
      artworkUrl: artworkUrl || currentMeta.artworkUrl || existingPage.artworkUrl || null,
      audioUrl: resolvedAudioUrl,
      duration: master.duration || 180,
      isPublic: existingPage.isPublic !== undefined ? existingPage.isPublic : true,
      allowDownload: allowDownload !== undefined ? !!allowDownload : !!existingPage.allowDownload,
      likesCount: existingPage.likesCount || 0,
      dislikesCount: existingPage.dislikesCount || 0,
      sharesCount: existingPage.sharesCount || 0,
      playCount: existingPage.playCount || 0,
      votes: existingPage.votes || {},
      createdAt: existingPage.createdAt || new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };

    const updatedMeta = {
      ...currentMeta,
      sharePage,
    };

    await prisma.master.update({
      where: { id: master.id },
      data: { metadata: updatedMeta },
    });

    return res.json({
      message: "Shareable media player saved",
      page: sharePage,
    });
  } catch (err) {
    console.error("[SHARE] createOrUpdate error:", err);
    return res.status(500).json({ message: "Failed to save share page" });
  }
};

// 2. Get Public Share Page by slug or ID
export const getSharePage = async (req, res) => {
  try {
    const { slugOrId } = req.params;

    // Search by master id directly or inspect metadata->sharePage->slug
    let master = await prisma.master.findUnique({
      where: { id: slugOrId },
    });

    if (!master) {
      // Find where metadata contains the slug
      const allMasters = await prisma.master.findMany({
        where: {
          metadata: { not: null },
        },
        select: {
          id: true,
          userId: true,
          outputUrl: true,
          sourceUrl: true,
          duration: true,
          sourceName: true,
          metadata: true,
        },
      });

      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === slugOrId || meta.sharePage.id === slugOrId);
      });
    }

    if (!master || !master.metadata || !master.metadata.sharePage) {
      return res.status(404).json({ message: "Shareable media player not found or deleted" });
    }

    const page = { ...master.metadata.sharePage };

    // If private and not owner
    if (!page.isPublic && (!req.userId || req.userId !== master.userId)) {
      return res.json({
        page: {
          id: page.id,
          slug: page.slug,
          isPublic: false,
          artist: page.artist,
          title: page.title,
        },
        message: "This player is currently private",
      });
    }

    // Fallback if artworkUrl is missing or invalid blob URL
    if (!page.artworkUrl || page.artworkUrl.startsWith("blob:")) {
      if (master.metadata?.artworkUrl && !master.metadata.artworkUrl.startsWith("blob:")) {
        page.artworkUrl = master.metadata.artworkUrl;
      }
    }

    // Ensure audioUrl is playable over HTTP
    if (!page.audioUrl || !page.audioUrl.startsWith("http")) {
      const proto = req.headers["x-forwarded-proto"] || req.protocol || "http";
      const host = req.headers["x-forwarded-host"] || req.headers.host || "localhost:8000";
      page.audioUrl = `${proto}://${host}/api/share-pages/${encodeURIComponent(page.slug || page.id)}/stream`;
    }

    // Get owner social links
    let socialLinks = {};
    try {
      const owner = await prisma.user.findUnique({
        where: { id: master.userId },
      });
      if (owner && master.metadata.socialLinks) {
        socialLinks = master.metadata.socialLinks;
      }
    } catch (e) {}

    return res.json({
      page,
      socialLinks,
    });
  } catch (err) {
    console.error("[SHARE] getSharePage error:", err);
    return res.status(500).json({ message: "Failed to load share page" });
  }
};

// 3. Patch Share Page settings (Owner only)
export const updateSharePageSettings = async (req, res) => {
  try {
    const { id } = req.params;
    const { isPublic, allowDownload, caption, artist, title } = req.body;

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master) {
      return res.status(404).json({ message: "Master not found" });
    }

    if (master.userId !== req.userId) {
      return res.status(403).json({ message: "Access denied" });
    }

    const currentMeta = (typeof master.metadata === "object" && master.metadata !== null)
      ? master.metadata
      : {};

    if (!currentMeta.sharePage) {
      return res.status(404).json({ message: "Share page does not exist for this master" });
    }

    const page = {
      ...currentMeta.sharePage,
      ...(isPublic !== undefined && { isPublic: !!isPublic }),
      ...(allowDownload !== undefined && { allowDownload: !!allowDownload }),
      ...(caption !== undefined && { caption: String(caption).trim() }),
      ...(artist !== undefined && { artist: String(artist).trim() }),
      ...(title !== undefined && { title: String(title).trim() }),
      updatedAt: new Date().toISOString(),
    };

    // Fallback if artworkUrl is missing or invalid blob URL
    if (!page.artworkUrl || page.artworkUrl.startsWith("blob:")) {
      if (master.metadata?.artworkUrl && !master.metadata.artworkUrl.startsWith("blob:")) {
        page.artworkUrl = master.metadata.artworkUrl;
      }
    }

    // Ensure audioUrl is playable over HTTP
    if (!page.audioUrl || !page.audioUrl.startsWith("http")) {
      const proto = req.headers["x-forwarded-proto"] || req.protocol || "http";
      const host = req.headers["x-forwarded-host"] || req.headers.host || "localhost:8000";
      page.audioUrl = `${proto}://${host}/api/share-pages/${encodeURIComponent(page.slug || page.id)}/stream`;
    }

    await prisma.master.update({
      where: { id: master.id },
      data: {
        metadata: {
          ...currentMeta,
          sharePage: page,
        },
      },
    });

    return res.json({ message: "Settings updated", page });
  } catch (err) {
    console.error("[SHARE] updateSharePageSettings error:", err);
    return res.status(500).json({ message: "Failed to update settings" });
  }
};

// 4. Delete Share Page only (Owner only - master is never deleted)
export const deleteSharePage = async (req, res) => {
  try {
    const { id } = req.params;

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master) {
      return res.status(404).json({ message: "Master not found" });
    }

    if (master.userId !== req.userId) {
      return res.status(403).json({ message: "Access denied" });
    }

    const currentMeta = (typeof master.metadata === "object" && master.metadata !== null)
      ? master.metadata
      : {};

    const updatedMeta = { ...currentMeta };
    delete updatedMeta.sharePage;

    await prisma.master.update({
      where: { id: master.id },
      data: { metadata: updatedMeta },
    });

    return res.json({ message: "Shareable media player deleted successfully" });
  } catch (err) {
    console.error("[SHARE] deleteSharePage error:", err);
    return res.status(500).json({ message: "Failed to delete share page" });
  }
};

// 5. Vote (Thumbs Up / Down) - Reversible
export const voteSharePage = async (req, res) => {
  try {
    const { id } = req.params;
    const { vote, visitorId } = req.body;

    if (!["up", "down", null].includes(vote)) {
      return res.status(400).json({ message: "Invalid vote" });
    }

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master || !master.metadata || !master.metadata.sharePage) {
      return res.status(404).json({ message: "Share page not found" });
    }

    const currentMeta = master.metadata;
    const page = { ...currentMeta.sharePage };
    const votes = { ...(page.votes || {}) };

    const previousVote = visitorId ? votes[visitorId] : null;

    if (previousVote === "up") page.likesCount = Math.max(0, (page.likesCount || 1) - 1);
    if (previousVote === "down") page.dislikesCount = Math.max(0, (page.dislikesCount || 1) - 1);

    if (vote === "up") {
      page.likesCount = (page.likesCount || 0) + 1;
      if (visitorId) votes[visitorId] = "up";
    } else if (vote === "down") {
      page.dislikesCount = (page.dislikesCount || 0) + 1;
      if (visitorId) votes[visitorId] = "down";
    } else if (visitorId) {
      delete votes[visitorId];
    }

    page.votes = votes;
    page.updatedAt = new Date().toISOString();

    await prisma.master.update({
      where: { id: master.id },
      data: {
        metadata: {
          ...currentMeta,
          sharePage: page,
        },
      },
    });

    return res.json({
      message: "Vote recorded",
      likesCount: page.likesCount,
      dislikesCount: page.dislikesCount,
      userVote: vote,
    });
  } catch (err) {
    console.error("[SHARE] vote error:", err);
    return res.status(500).json({ message: "Failed to record vote" });
  }
};

// 6. Share Modal Opened / Incremented
export const incrementShareCount = async (req, res) => {
  try {
    const { id } = req.params;

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master || !master.metadata || !master.metadata.sharePage) {
      return res.status(404).json({ message: "Share page not found" });
    }

    const currentMeta = master.metadata;
    const page = { ...currentMeta.sharePage };
    page.sharesCount = (page.sharesCount || 0) + 1;

    await prisma.master.update({
      where: { id: master.id },
      data: {
        metadata: {
          ...currentMeta,
          sharePage: page,
        },
      },
    });

    return res.json({
      message: "Share recorded",
      sharesCount: page.sharesCount,
    });
  } catch (err) {
    console.error("[SHARE] incrementShareCount error:", err);
    return res.status(500).json({ message: "Failed to increment share" });
  }
};

// 7. Track Played
export const incrementPlayCount = async (req, res) => {
  try {
    const { id } = req.params;

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master || !master.metadata || !master.metadata.sharePage) {
      return res.status(404).json({ message: "Share page not found" });
    }

    const currentMeta = master.metadata;
    const page = { ...currentMeta.sharePage };
    page.playCount = (page.playCount || 0) + 1;

    await prisma.master.update({
      where: { id: master.id },
      data: {
        metadata: {
          ...currentMeta,
          sharePage: page,
        },
      },
    });

    return res.json({
      message: "Play count updated",
      playCount: page.playCount,
    });
  } catch (err) {
    console.error("[SHARE] incrementPlayCount error:", err);
    return res.status(500).json({ message: "Failed to increment play" });
  }
};

// 8. Stream Audio for Web Playback with Range Header Support
export const streamShareAudio = async (req, res) => {
  try {
    const { id } = req.params;

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master) {
      return res.status(404).json({ message: "Audio not found" });
    }

    const page = master.metadata?.sharePage;
    if (page && !page.isPublic && (!req.userId || req.userId !== master.userId)) {
      return res.status(403).json({ message: "This track is private" });
    }

    // Remote HTTP output
    if (master.outputUrl && master.outputUrl.startsWith("http")) {
      return res.redirect(master.outputUrl);
    }

    // Local file on disk
    let diskPath = resolveLocalWavPath(master);
    if (!diskPath || !fs.existsSync(diskPath)) {
      const standardOut = path.join(MASTER_TMP_DIR, `${master.id}.wav`);
      if (fs.existsSync(standardOut)) {
        diskPath = standardOut;
      } else {
        const standardSrc = path.join(MASTER_TMP_DIR, `${master.id}-source.wav`);
        if (fs.existsSync(standardSrc)) {
          diskPath = standardSrc;
        } else if (master.sourceUrl?.startsWith("local://")) {
          const key = master.sourceUrl.replace("local://", "");
          const candidate = path.join(MASTER_TMP_DIR, path.basename(key));
          if (fs.existsSync(candidate)) diskPath = candidate;
        }
      }
    }

    if (!diskPath || !fs.existsSync(diskPath)) {
      return res.status(404).json({ message: "Audio file not available on server" });
    }

    const stat = fs.statSync(diskPath);
    const fileSize = stat.size;
    const range = req.headers.range;

    if (range) {
      const parts = range.replace(/bytes=/, "").split("-");
      const start = parseInt(parts[0], 10);
      const end = parts[1] ? parseInt(parts[1], 10) : fileSize - 1;
      const chunksize = end - start + 1;
      const file = fs.createReadStream(diskPath, { start, end });
      const head = {
        "Content-Range": `bytes ${start}-${end}/${fileSize}`,
        "Accept-Ranges": "bytes",
        "Content-Length": chunksize,
        "Content-Type": "audio/wav",
      };
      res.writeHead(206, head);
      file.pipe(res);
    } else {
      const head = {
        "Content-Length": fileSize,
        "Accept-Ranges": "bytes",
        "Content-Type": "audio/wav",
      };
      res.writeHead(200, head);
      fs.createReadStream(diskPath).pipe(res);
    }
  } catch (err) {
    console.error("[SHARE] streamShareAudio error:", err);
    if (!res.headersSent) res.status(500).json({ message: "Streaming failed" });
  }
};

// 9. Download MP3 / Master Audio
export const downloadShareMp3 = async (req, res) => {
  try {
    const { id } = req.params;

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master || !master.metadata || !master.metadata.sharePage) {
      return res.status(404).json({ message: "Share page not found" });
    }

    const page = master.metadata.sharePage;

    if (!page.isPublic || !page.allowDownload) {
      return res.status(403).json({ message: "Downloads are not enabled for this track" });
    }

    if (master.outputUrl && master.outputUrl.startsWith("http")) {
      return res.redirect(master.outputUrl);
    }

    let diskPath = resolveLocalWavPath(master);
    if (!diskPath || !fs.existsSync(diskPath)) {
      const standardOut = path.join(MASTER_TMP_DIR, `${master.id}.wav`);
      if (fs.existsSync(standardOut)) {
        diskPath = standardOut;
      } else {
        const standardSrc = path.join(MASTER_TMP_DIR, `${master.id}-source.wav`);
        if (fs.existsSync(standardSrc)) diskPath = standardSrc;
      }
    }

    if (diskPath && fs.existsSync(diskPath)) {
      const filename = `${page.artist || "Artist"} - ${page.title || "Track"}.wav`;
      res.setHeader("Content-Disposition", `attachment; filename="${encodeURIComponent(filename)}"`);
      res.setHeader("Content-Type", "audio/wav");
      res.setHeader("Content-Length", String(fs.statSync(diskPath).size));
      return fs.createReadStream(diskPath).pipe(res);
    }

    return res.status(404).json({ message: "Audio file unavailable" });
  } catch (err) {
    console.error("[SHARE] downloadShareMp3 error:", err);
    return res.status(500).json({ message: "Download failed" });
  }
};
