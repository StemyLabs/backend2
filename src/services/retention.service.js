import { env } from "../config/env.js";
import { prisma } from "../lib/prisma.js";
import { deleteByUrl } from "./storage.service.js";

const ACTIVE_STATUSES = new Set(["ACTIVE", "TRIALING"]);
const PURGED_URL = "purged://";

export const calculateFilesPurgeAt = (accessEndedAt, retentionDays = env.FILE_RETENTION_DAYS) => {
  if (!(accessEndedAt instanceof Date) || Number.isNaN(accessEndedAt.getTime())) {
    throw new Error("accessEndedAt must be a valid Date");
  }
  if (!Number.isFinite(retentionDays) || retentionDays < 0) {
    throw new Error("retentionDays must be a non-negative number");
  }

  const purgeAt = new Date(accessEndedAt);
  purgeAt.setUTCDate(purgeAt.getUTCDate() + retentionDays);
  return purgeAt;
};

export const resolveAccessEndedAt = (stripeSub, fallback = new Date()) => {
  if (stripeSub?.ended_at) {
    return new Date(stripeSub.ended_at * 1000);
  }
  if (stripeSub?.canceled_at) {
    return new Date(stripeSub.canceled_at * 1000);
  }
  if (stripeSub?.current_period_end) {
    return new Date(stripeSub.current_period_end * 1000);
  }
  return fallback;
};

export const scheduleFileRetention = async ({
  userId,
  accessEndedAt,
  retentionDays = env.FILE_RETENTION_DAYS,
  db = prisma,
} = {}) => {
  if (!userId) {
    throw new Error("userId is required to schedule file retention");
  }

  const endedAt = accessEndedAt instanceof Date ? accessEndedAt : new Date(accessEndedAt);
  if (Number.isNaN(endedAt.getTime())) {
    throw new Error("accessEndedAt must be a valid date");
  }

  const filesPurgeAt = calculateFilesPurgeAt(endedAt, retentionDays);

  const subscription = await db.subscription.findFirst({
    where: { userId },
    orderBy: { createdAt: "desc" },
  });

  if (!subscription) {
    return { scheduled: false, reason: "no_subscription" };
  }

  if (ACTIVE_STATUSES.has(subscription.status)) {
    return { scheduled: false, reason: "subscription_still_active" };
  }

  if (subscription.filesPurgedAt) {
    return { scheduled: false, reason: "already_purged" };
  }

  await db.subscription.update({
    where: { id: subscription.id },
    data: {
      accessEndedAt: endedAt,
      filesPurgeAt,
      cancelAtPeriodEnd: subscription.cancelAtPeriodEnd,
    },
  });

  return { scheduled: true, filesPurgeAt, accessEndedAt: endedAt };
};

export const clearFileRetention = async ({ userId, db = prisma } = {}) => {
  if (!userId) {
    throw new Error("userId is required to clear file retention");
  }

  const result = await db.subscription.updateMany({
    where: {
      userId,
      filesPurgedAt: null,
    },
    data: {
      accessEndedAt: null,
      filesPurgeAt: null,
    },
  });

  return { cleared: result.count > 0, count: result.count };
};

const collectMasterStorageUrls = (master) => {
  const urls = [];
  if (master.sourceUrl && master.sourceUrl !== PURGED_URL) urls.push(master.sourceUrl);
  if (master.outputUrl && master.outputUrl !== PURGED_URL) urls.push(master.outputUrl);

  const artworkUrl = master.metadata?.artworkUrl;
  if (artworkUrl && artworkUrl !== PURGED_URL) urls.push(artworkUrl);

  return urls;
};

export const purgeMasterFiles = async ({ master, deleteFile = deleteByUrl } = {}) => {
  if (!master?.id) {
    throw new Error("master is required");
  }
  if (master.filesPurgedAt) {
    return { purged: false, reason: "already_purged", masterId: master.id };
  }

  const urls = collectMasterStorageUrls(master);
  const deleted = [];
  const failed = [];

  for (const url of urls) {
    try {
      const ok = await deleteFile(url);
      if (ok) deleted.push(url);
    } catch (err) {
      failed.push({ url, error: err.message });
    }
  }

  if (failed.length > 0) {
    return {
      purged: false,
      reason: "delete_failed",
      masterId: master.id,
      deleted,
      failed,
    };
  }

  return {
    purged: true,
    masterId: master.id,
    deleted,
    updateData: {
      sourceUrl: PURGED_URL,
      outputUrl: master.outputUrl ? PURGED_URL : null,
      metadata: master.metadata?.artworkUrl
        ? { ...master.metadata, artworkUrl: PURGED_URL }
        : master.metadata,
      filesPurgedAt: new Date(),
    },
  };
};

export const userHasActiveSubscription = async (userId, db = prisma) => {
  const active = await db.subscription.findFirst({
    where: {
      userId,
      status: { in: [...ACTIVE_STATUSES] },
    },
    select: { id: true },
  });
  return Boolean(active);
};

export const findSubscriptionsDueForPurge = async ({ now = new Date(), db = prisma } = {}) => {
  if (!(now instanceof Date) || Number.isNaN(now.getTime())) {
    throw new Error("now must be a valid Date");
  }

  return db.subscription.findMany({
    where: {
      status: "CANCELED",
      filesPurgeAt: { lte: now },
      filesPurgedAt: null,
    },
    select: {
      id: true,
      userId: true,
      filesPurgeAt: true,
      accessEndedAt: true,
    },
  });
};

export const runRetentionPurge = async ({
  now = new Date(),
  db = prisma,
  deleteFile = deleteByUrl,
  batchSize = env.FILE_RETENTION_BATCH_SIZE,
} = {}) => {
  if (!(now instanceof Date) || Number.isNaN(now.getTime())) {
    throw new Error("now must be a valid Date");
  }

  const due = await findSubscriptionsDueForPurge({ now, db });
  const summary = {
    checked: due.length,
    purgedUsers: 0,
    purgedMasters: 0,
    skippedUsers: 0,
    errors: [],
  };

  for (const subscription of due) {
    try {
      const stillActive = await userHasActiveSubscription(subscription.userId, db);
      if (stillActive) {
        await clearFileRetention({ userId: subscription.userId, db });
        summary.skippedUsers += 1;
        continue;
      }

      const masters = await db.master.findMany({
        where: {
          userId: subscription.userId,
          filesPurgedAt: null,
        },
      });

      let allMastersPurged = true;

      for (const master of masters) {
        const result = await purgeMasterFiles({ master, deleteFile });
        if (result.purged) {
          await db.master.update({
            where: { id: master.id },
            data: result.updateData,
          });
          summary.purgedMasters += 1;
        } else if (result.reason === "delete_failed") {
          allMastersPurged = false;
          summary.errors.push({
            userId: subscription.userId,
            masterId: master.id,
            failed: result.failed,
          });
        }
      }

      if (allMastersPurged) {
        await db.subscription.update({
          where: { id: subscription.id },
          data: { filesPurgedAt: now },
        });
        summary.purgedUsers += 1;
      }
    } catch (err) {
      summary.errors.push({
        userId: subscription.userId,
        error: err.message,
      });
    }
  }

  return summary;
};

export const syncRetentionFromSubscriptionUpdate = async ({
  userId,
  status,
  stripeSub,
  db = prisma,
  retentionDays = env.FILE_RETENTION_DAYS,
} = {}) => {
  if (!userId) {
    throw new Error("userId is required");
  }

  const cancelAtPeriodEnd = Boolean(stripeSub?.cancel_at_period_end);

  await db.subscription.updateMany({
    where: { userId },
    data: { cancelAtPeriodEnd },
  });

  if (ACTIVE_STATUSES.has(status)) {
    await clearFileRetention({ userId, db });
    return { action: "cleared", cancelAtPeriodEnd };
  }

  if (status === "CANCELED") {
    const accessEndedAt = resolveAccessEndedAt(stripeSub);
    const result = await scheduleFileRetention({
      userId,
      accessEndedAt,
      retentionDays,
      db,
    });
    return { action: "scheduled", cancelAtPeriodEnd, ...result };
  }

  return { action: "unchanged", cancelAtPeriodEnd };
};

export { PURGED_URL };
