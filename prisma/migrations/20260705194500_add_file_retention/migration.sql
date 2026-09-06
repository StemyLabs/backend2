-- AlterTable
ALTER TABLE "Subscription" ADD COLUMN "cancelAtPeriodEnd" BOOLEAN NOT NULL DEFAULT false;
ALTER TABLE "Subscription" ADD COLUMN "accessEndedAt" TIMESTAMP(3);
ALTER TABLE "Subscription" ADD COLUMN "filesPurgeAt" TIMESTAMP(3);
ALTER TABLE "Subscription" ADD COLUMN "filesPurgedAt" TIMESTAMP(3);

-- AlterTable
ALTER TABLE "Master" ADD COLUMN "filesPurgedAt" TIMESTAMP(3);
