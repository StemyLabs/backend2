import { prisma } from "../lib/prisma.js";

// Helper to create clean URL-safe slugs
function slugify(text) {
  return String(text || "")
    .toLowerCase()
    .trim()
    .replace(/[^\w\s-]/g, "")
    .replace(/[\s_-]+/g, "-")
    .replace(/^-+|-+$/g, "");
}

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

    const sharePage = {
      id: master.id,
      masterId: master.id,
      slug,
      artist: safeArtist,
      title: safeTitle,
      caption: (caption !== undefined ? caption : existingPage.caption || "").trim(),
      artworkUrl: artworkUrl || currentMeta.artworkUrl || existingPage.artworkUrl || null,
      audioUrl: master.outputUrl,
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

    // Also fetch owner's social links
    const user = await prisma.user.findUnique({
      where: { id: req.userId },
      select: { displayName: true },
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
          status: "COMPLETE",
          metadata: { not: null },
        },
        select: {
          id: true,
          userId: true,
          outputUrl: true,
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

    const page = master.metadata.sharePage;

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

    // Get owner social links
    let socialLinks = {};
    try {
      const owner = await prisma.user.findUnique({
        where: { id: master.userId },
      });
      // Retrieve social links if stored
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
        where: { status: "COMPLETE", metadata: { not: null } },
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
        where: { status: "COMPLETE", metadata: { not: null } },
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

// 5. Public Reversible Thumbs Up / Down Voting (Client Item #11)
export const voteSharePage = async (req, res) => {
  try {
    const { id } = req.params;
    const { vote, visitorId } = req.body; // 'up', 'down', or 'clear'

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { status: "COMPLETE", metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master || !master.metadata || !master.metadata.sharePage) {
      return res.status(404).json({ message: "Share page not found" });
    }

    const meta = master.metadata;
    const page = meta.sharePage;
    const vId = visitorId || req.ip || "guest";
    const votes = page.votes || {};
    const previousVote = votes[vId] || null;

    let likesCount = page.likesCount || 0;
    let dislikesCount = page.dislikesCount || 0;

    let newVote = null;

    if (vote === "up") {
      if (previousVote === "up") {
        // Toggle off
        likesCount = Math.max(0, likesCount - 1);
        delete votes[vId];
        newVote = null;
      } else {
        if (previousVote === "down") dislikesCount = Math.max(0, dislikesCount - 1);
        likesCount += 1;
        votes[vId] = "up";
        newVote = "up";
      }
    } else if (vote === "down") {
      if (previousVote === "down") {
        // Toggle off
        dislikesCount = Math.max(0, dislikesCount - 1);
        delete votes[vId];
        newVote = null;
      } else {
        if (previousVote === "up") likesCount = Math.max(0, likesCount - 1);
        dislikesCount += 1;
        votes[vId] = "down";
        newVote = "down";
      }
    } else if (vote === "clear") {
      if (previousVote === "up") likesCount = Math.max(0, likesCount - 1);
      if (previousVote === "down") dislikesCount = Math.max(0, dislikesCount - 1);
      delete votes[vId];
      newVote = null;
    }

    page.likesCount = likesCount;
    page.dislikesCount = dislikesCount;
    page.votes = votes;

    await prisma.master.update({
      where: { id },
      data: {
        metadata: {
          ...meta,
          sharePage: page,
        },
      },
    });

    return res.json({
      likesCount,
      dislikesCount,
      currentVote: newVote,
    });
  } catch (err) {
    console.error("[SHARE] voteSharePage error:", err);
    return res.status(500).json({ message: "Failed to record vote" });
  }
};

// 6. Public Share Count Incrementer (Client Item #12)
export const incrementShareCount = async (req, res) => {
  try {
    const { id } = req.params;

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { status: "COMPLETE", metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master || !master.metadata || !master.metadata.sharePage) {
      return res.status(404).json({ message: "Share page not found" });
    }

    const meta = master.metadata;
    const page = meta.sharePage;
    page.sharesCount = (page.sharesCount || 0) + 1;

    await prisma.master.update({
      where: { id },
      data: {
        metadata: {
          ...meta,
          sharePage: page,
        },
      },
    });

    return res.json({ sharesCount: page.sharesCount });
  } catch (err) {
    console.error("[SHARE] incrementShareCount error:", err);
    return res.status(500).json({ message: "Failed to record share" });
  }
};

// 7. Public Play Count Incrementer
export const incrementPlayCount = async (req, res) => {
  try {
    const { id } = req.params;

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { status: "COMPLETE", metadata: { not: null } },
      });
      master = allMasters.find((m) => {
        const meta = m.metadata;
        return meta && meta.sharePage && (meta.sharePage.slug === id || meta.sharePage.id === id);
      });
    }

    if (!master || !master.metadata || !master.metadata.sharePage) {
      return res.status(404).json({ message: "Share page not found" });
    }

    const meta = master.metadata;
    const page = meta.sharePage;
    page.playCount = (page.playCount || 0) + 1;

    await prisma.master.update({
      where: { id },
      data: {
        metadata: {
          ...meta,
          sharePage: page,
        },
      },
    });

    return res.json({ playCount: page.playCount });
  } catch (err) {
    console.error("[SHARE] incrementPlayCount error:", err);
    return res.status(500).json({ message: "Failed to record play" });
  }
};

// 8. Public MP3 Download (serves 320 kbps MP3 only if allowDownload && isPublic)
export const downloadShareMp3 = async (req, res) => {
  try {
    const { id } = req.params;

    let master = await prisma.master.findUnique({
      where: { id },
    });

    if (!master) {
      const allMasters = await prisma.master.findMany({
        where: { status: "COMPLETE", metadata: { not: null } },
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

    // Redirect or pipe MP3 download
    const filename = `${page.artist || "Artist"} - ${page.title || "Track"}.mp3`;
    res.setHeader("Content-Disposition", `attachment; filename="${encodeURIComponent(filename)}"`);
    res.setHeader("Content-Type", "audio/mpeg");

    // Redirect to outputUrl or return JSON
    if (page.audioUrl) {
      return res.redirect(page.audioUrl);
    }

    return res.status(404).json({ message: "Audio file unavailable" });
  } catch (err) {
    console.error("[SHARE] downloadShareMp3 error:", err);
    return res.status(500).json({ message: "Download failed" });
  }
};
