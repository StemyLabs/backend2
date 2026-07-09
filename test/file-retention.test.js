import test from "node:test";
import assert from "node:assert/strict";
import {
  calculateFilesPurgeAt,
  resolveAccessEndedAt,
  scheduleFileRetention,
  clearFileRetention,
  purgeMasterFiles,
  runRetentionPurge,
  syncRetentionFromSubscriptionUpdate,
  PURGED_URL,
} from "../src/services/retention.service.js";
import { extractStorageKey } from "../src/services/storage.service.js";
import cron from "node-cron";
import { env } from "../src/config/env.js";

const makeMockDb = (overrides = {}) => {
  const state = {
    subscription: overrides.subscription ?? null,
    subscriptionsDue: overrides.subscriptionsDue ?? [],
    masters: overrides.masters ?? [],
    activeSubscription: overrides.activeSubscription ?? null,
    updates: [],
  };

  return {
    state,
    subscription: {
      findFirst: async ({ where, orderBy } = {}) => {
        if (where?.status?.in) {
          return state.activeSubscription;
        }
        if (where?.userId && state.subscription?.userId === where.userId) {
          return state.subscription;
        }
        return state.subscription;
      },
      findMany: async () => state.subscriptionsDue,
      update: async ({ where, data }) => {
        state.updates.push({ type: "subscription.update", where, data });
        if (state.subscription?.id === where.id) {
          state.subscription = { ...state.subscription, ...data };
        }
        return state.subscription;
      },
      updateMany: async ({ where, data }) => {
        state.updates.push({ type: "subscription.updateMany", where, data });
        if (state.subscription && where.userId === state.subscription.userId) {
          state.subscription = { ...state.subscription, ...data };
        }
        return { count: 1 };
      },
    },
    master: {
      findMany: async ({ where } = {}) =>
        state.masters.filter(
          (m) =>
            m.userId === where.userId &&
            (where.filesPurgedAt === null ? !m.filesPurgedAt : true),
        ),
      update: async ({ where, data }) => {
        state.updates.push({ type: "master.update", where, data });
        const idx = state.masters.findIndex((m) => m.id === where.id);
        if (idx >= 0) {
          state.masters[idx] = { ...state.masters[idx], ...data };
        }
      },
    },
  };
};

test("calculateFilesPurgeAt adds retention days in UTC", () => {
  const accessEndedAt = new Date("2026-01-15T12:00:00.000Z");
  const purgeAt = calculateFilesPurgeAt(accessEndedAt, 30);
  assert.equal(purgeAt.toISOString(), "2026-02-14T12:00:00.000Z");
});

test("calculateFilesPurgeAt rejects invalid inputs", () => {
  assert.throws(() => calculateFilesPurgeAt(new Date("invalid"), 30));
  assert.throws(() => calculateFilesPurgeAt(new Date(), -1));
});

test("resolveAccessEndedAt prefers Stripe ended_at", () => {
  const ended = resolveAccessEndedAt({
    ended_at: 1700000000,
    canceled_at: 1600000000,
    current_period_end: 1500000000,
  });
  assert.equal(ended.toISOString(), new Date(1700000000 * 1000).toISOString());
});

test("scheduleFileRetention sets purge date for canceled subscription", async () => {
  const accessEndedAt = new Date("2026-01-01T00:00:00.000Z");
  const db = makeMockDb({
    subscription: {
      id: "sub-1",
      userId: "user-1",
      status: "CANCELED",
      cancelAtPeriodEnd: false,
      filesPurgedAt: null,
    },
  });

  const result = await scheduleFileRetention({
    userId: "user-1",
    accessEndedAt,
    retentionDays: 30,
    db,
  });

  assert.equal(result.scheduled, true);
  assert.equal(result.filesPurgeAt.toISOString(), "2026-01-31T00:00:00.000Z");
  assert.equal(db.state.subscription.filesPurgeAt.toISOString(), "2026-01-31T00:00:00.000Z");
});

test("scheduleFileRetention skips active subscriptions", async () => {
  const db = makeMockDb({
    subscription: {
      id: "sub-1",
      userId: "user-1",
      status: "ACTIVE",
      filesPurgedAt: null,
    },
  });

  const result = await scheduleFileRetention({
    userId: "user-1",
    accessEndedAt: new Date(),
    db,
  });

  assert.equal(result.scheduled, false);
  assert.equal(result.reason, "subscription_still_active");
});

test("clearFileRetention removes pending purge timestamps", async () => {
  const db = makeMockDb({
    subscription: {
      id: "sub-1",
      userId: "user-1",
      status: "ACTIVE",
      accessEndedAt: new Date(),
      filesPurgeAt: new Date(),
      filesPurgedAt: null,
      cancelAtPeriodEnd: true,
    },
  });

  const result = await clearFileRetention({ userId: "user-1", db });
  assert.equal(result.cleared, true);
  assert.equal(db.state.subscription.accessEndedAt, null);
  assert.equal(db.state.subscription.filesPurgeAt, null);
  assert.equal(db.state.subscription.cancelAtPeriodEnd, true);
});

test("purgeMasterFiles deletes source, output, and artwork URLs", async () => {
  const deleted = [];
  const master = {
    id: "m-1",
    sourceUrl: "https://cdn.example.com/masters/u1/source.wav",
    outputUrl: "https://cdn.example.com/masters/u1/output.wav",
    metadata: { artworkUrl: "https://cdn.example.com/artwork/u1/cover.jpg" },
    filesPurgedAt: null,
  };

  const result = await purgeMasterFiles({
    master,
    deleteFile: async (url) => {
      deleted.push(url);
      return true;
    },
  });

  assert.equal(result.purged, true);
  assert.equal(deleted.length, 3);
  assert.equal(result.updateData.sourceUrl, PURGED_URL);
  assert.equal(result.updateData.outputUrl, PURGED_URL);
  assert.equal(result.updateData.metadata.artworkUrl, PURGED_URL);
});

test("runRetentionPurge purges due users and skips reactivated users", async () => {
  const now = new Date("2026-03-01T00:00:00.000Z");
  const db = makeMockDb({
    subscriptionsDue: [
      {
        id: "sub-due",
        userId: "user-due",
        filesPurgeAt: new Date("2026-02-01T00:00:00.000Z"),
      },
      {
        id: "sub-active",
        userId: "user-active",
        filesPurgeAt: new Date("2026-02-01T00:00:00.000Z"),
      },
    ],
    masters: [
      {
        id: "m-1",
        userId: "user-due",
        sourceUrl: "https://cdn.example.com/masters/u1/source.wav",
        outputUrl: "https://cdn.example.com/masters/u1/output.wav",
        metadata: {},
        filesPurgedAt: null,
      },
    ],
    subscription: {
      id: "sub-due",
      userId: "user-due",
      status: "CANCELED",
      filesPurgedAt: null,
    },
  });

  const originalFindFirst = db.subscription.findFirst;
  db.subscription.findFirst = async (args) => {
    if (args?.where?.status?.in) {
      if (args.where.userId === "user-active") {
        return { id: "sub-active-live", userId: "user-active", status: "ACTIVE" };
      }
      return null;
    }
    return originalFindFirst(args);
  };

  const deletedUrls = [];
  const summary = await runRetentionPurge({
    now,
    db,
    deleteFile: async (url) => {
      deletedUrls.push(url);
      return true;
    },
  });

  assert.equal(summary.checked, 2);
  assert.equal(summary.purgedUsers, 1);
  assert.equal(summary.purgedMasters, 1);
  assert.equal(summary.skippedUsers, 1);
  assert.equal(deletedUrls.length, 2);
  assert.ok(db.state.updates.some((u) => u.type === "subscription.update" && u.data.filesPurgedAt));
});

test("runRetentionPurge does not mark user purged when delete fails", async () => {
  const db = makeMockDb({
    subscriptionsDue: [
      {
        id: "sub-due",
        userId: "user-due",
        filesPurgeAt: new Date("2026-01-01T00:00:00.000Z"),
      },
    ],
    masters: [
      {
        id: "m-1",
        userId: "user-due",
        sourceUrl: "https://cdn.example.com/masters/u1/source.wav",
        outputUrl: null,
        metadata: {},
        filesPurgedAt: null,
      },
    ],
    subscription: {
      id: "sub-due",
      userId: "user-due",
      status: "CANCELED",
      filesPurgedAt: null,
    },
  });

  const summary = await runRetentionPurge({
    now: new Date("2026-02-01T00:00:00.000Z"),
    db,
    deleteFile: async () => {
      throw new Error("R2 unavailable");
    },
  });

  assert.equal(summary.purgedUsers, 0);
  assert.equal(summary.errors.length, 1);
  assert.equal(summary.errors[0].masterId, "m-1");
});

test("syncRetentionFromSubscriptionUpdate clears on active and schedules on canceled", async () => {
  const db = makeMockDb({
    subscription: {
      id: "sub-1",
      userId: "user-1",
      status: "CANCELED",
      filesPurgedAt: null,
      cancelAtPeriodEnd: false,
    },
  });

  const cleared = await syncRetentionFromSubscriptionUpdate({
    userId: "user-1",
    status: "ACTIVE",
    stripeSub: { cancel_at_period_end: true },
    db,
  });
  assert.equal(cleared.action, "cleared");
  assert.equal(db.state.subscription.cancelAtPeriodEnd, true);

  db.state.subscription.status = "CANCELED";
  const scheduled = await syncRetentionFromSubscriptionUpdate({
    userId: "user-1",
    status: "CANCELED",
    stripeSub: {
      ended_at: Math.floor(new Date("2026-01-01T00:00:00.000Z").getTime() / 1000),
      cancel_at_period_end: false,
    },
    db,
    retentionDays: 30,
  });
  assert.equal(scheduled.action, "scheduled");
  assert.equal(scheduled.scheduled, true);
  assert.equal(scheduled.filesPurgeAt.toISOString(), "2026-01-31T00:00:00.000Z");
});

test("extractStorageKey parses r2 URLs and skips local paths", () => {
  if (env.R2_BUCKET) {
    assert.equal(
      extractStorageKey(`r2://${env.R2_BUCKET}/masters/user/file.wav`),
      "masters/user/file.wav",
    );
  }
  if (env.R2_PUBLIC_BASE_URL) {
    assert.equal(
      extractStorageKey(`${env.R2_PUBLIC_BASE_URL}/masters/user/file.wav`),
      "masters/user/file.wav",
    );
  }
  assert.equal(extractStorageKey("local://masters/user/file.wav"), null);
});

test("file retention cron schedule expression is valid by default", () => {
  assert.equal(cron.validate("0 3 * * *"), true);
  assert.equal(cron.validate("not-a-cron"), false);
});
