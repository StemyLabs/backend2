import cron from "node-cron";
import { env } from "../config/env.js";
import { runRetentionPurge } from "../services/retention.service.js";

const isValidCronExpression = (expression) => cron.validate(expression);

export const startFileRetentionCron = () => {
  if (!env.FILE_RETENTION_CRON_ENABLED) {
    console.log("📅 File retention cron disabled (FILE_RETENTION_CRON_ENABLED=false)");
    return;
  }

  const schedule = env.FILE_RETENTION_CRON_SCHEDULE;
  if (!isValidCronExpression(schedule)) {
    console.error(
      `[FileRetention] Invalid cron schedule "${schedule}" — cron not started`,
    );
    return;
  }

  cron.schedule(schedule, async () => {
    const started = Date.now();
    console.log("[FileRetention] Starting scheduled purge job");

    try {
      const summary = await runRetentionPurge({ now: new Date() });
      console.log("[FileRetention] Purge job finished", {
        ...summary,
        durationMs: Date.now() - started,
      });
    } catch (error) {
      console.error("[FileRetention] Purge job failed:", error);
    }
  });

  console.log(
    `📅 File retention cron scheduled (${schedule}, ${env.FILE_RETENTION_DAYS}-day grace period)`,
  );
};
