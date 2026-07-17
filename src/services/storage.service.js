import {
  S3Client,
  PutObjectCommand,
  GetObjectCommand,
  DeleteObjectCommand,
} from "@aws-sdk/client-s3";
import { getSignedUrl } from "@aws-sdk/s3-request-presigner";
import fs from "fs";
import fsp from "fs/promises";
import path from "path";
import { pipeline } from "stream/promises";
import { env } from "../config/env.js";
import { MASTER_TMP_DIR } from "../utils/master-temp.js";

const useR2 =
  !!env.R2_ENDPOINT &&
  !!env.R2_ACCESS_KEY_ID &&
  !!env.R2_SECRET_ACCESS_KEY &&
  !!env.R2_BUCKET;

/** On-disk fallback when R2 is not configured (local dev). */
export const LOCAL_STORAGE_DIR = path.join(MASTER_TMP_DIR, "storage");

const localStoragePath = (key) => {
  const normalizedKey = String(key || "").replace(/^\/+/, "");
  return path.join(LOCAL_STORAGE_DIR, normalizedKey);
};

export const readLocalStorage = async (storageUrl) => {
  if (!storageUrl?.startsWith("local://")) return null;
  const filePath = localStoragePath(storageUrl.replace("local://", ""));
  if (!fs.existsSync(filePath)) return null;
  return fsp.readFile(filePath);
};

const s3 = useR2
  ? new S3Client({
      region: env.R2_REGION || "auto",
      endpoint: env.R2_ENDPOINT,
      credentials: {
        accessKeyId: env.R2_ACCESS_KEY_ID,
        secretAccessKey: env.R2_SECRET_ACCESS_KEY,
      },
    })
  : null;

const toPublicUrl = (key) => {
  const normalizedKey = String(key || "").replace(/^\/+/, "");
  const base = env.R2_PUBLIC_BASE_URL?.replace(/\/+$/, "");
  if (base) {
    return `${base}/${normalizedKey}`;
  }
  const endpointHost = env.R2_ENDPOINT?.replace(/^https?:\/\//, "").replace(
    /\/+$/,
    "",
  );
  if (endpointHost && env.R2_BUCKET) {
    return `https://${env.R2_BUCKET}.${endpointHost}/${normalizedKey}`;
  }
  return null;
};

export const uploadBuffer = async ({ key, body, contentType }) => {
  if (!s3) {
    const filePath = localStoragePath(key);
    await fsp.mkdir(path.dirname(filePath), { recursive: true });
    await fsp.writeFile(filePath, body);
    return `local://${String(key || "").replace(/^\/+/, "")}`;
  }

  await s3.send(
    new PutObjectCommand({
      Bucket: env.R2_BUCKET,
      Key: key,
      Body: body,
      ContentType: contentType || "application/octet-stream",
    }),
  );

  const publicUrl = toPublicUrl(key);
  if (publicUrl) return publicUrl;

  // Fallback: construct R2 public URL manually
  if (env.R2_PUBLIC_BASE_URL) {
    return `${env.R2_PUBLIC_BASE_URL.replace(/\/+$/, "")}/${key.replace(/^\/+/, "")}`;
  }

  return `r2://${env.R2_BUCKET}/${key}`;
};

export const uploadStream = async ({ key, stream, contentType, contentLength }) => {
  if (!s3) {
    const filePath = localStoragePath(key);
    await fsp.mkdir(path.dirname(filePath), { recursive: true });
    await pipeline(stream, fs.createWriteStream(filePath));
    return `local://${String(key || "").replace(/^\/+/, "")}`;
  }

  await s3.send(
    new PutObjectCommand({
      Bucket: env.R2_BUCKET,
      Key: key,
      Body: stream,
      ContentType: contentType || "application/octet-stream",
      ContentLength: contentLength,
    }),
  );

  return toPublicUrl(key) || `r2://${env.R2_BUCKET}/${key}`;
};

/** Extract the R2/S3 object key from a stored URL. Returns null for local or invalid URLs. */
export const extractStorageKey = (storageUrl) => {
  if (!storageUrl || storageUrl.startsWith("local://")) {
    return null;
  }

  if (storageUrl.startsWith(`r2://${env.R2_BUCKET}/`)) {
    return storageUrl.replace(`r2://${env.R2_BUCKET}/`, "");
  }

  if (env.R2_PUBLIC_BASE_URL && storageUrl.includes(env.R2_PUBLIC_BASE_URL)) {
    return storageUrl.replace(`${env.R2_PUBLIC_BASE_URL.replace(/\/+$/, "")}/`, "");
  }

  if (storageUrl.includes(".r2.dev")) {
    const url = new URL(storageUrl);
    return url.pathname.replace(/^\//, "");
  }

  if (storageUrl.includes(`${env.R2_BUCKET}.`)) {
    const url = new URL(storageUrl);
    return url.pathname.replace(/^\//, "");
  }

  try {
    const url = new URL(storageUrl);
    return url.pathname.replace(/^\//, "") || null;
  } catch {
    return null;
  }
};

export const deleteLocalStorage = async (storageUrl) => {
  if (!storageUrl?.startsWith("local://")) return false;
  const filePath = localStoragePath(storageUrl.replace("local://", ""));
  if (!fs.existsSync(filePath)) return false;
  await fsp.unlink(filePath);
  return true;
};

export const deleteObject = async (key) => {
  if (!key) return false;
  if (!s3) return false;

  await s3.send(
    new DeleteObjectCommand({
      Bucket: env.R2_BUCKET,
      Key: key,
    }),
  );
  return true;
};

export const deleteByUrl = async (storageUrl) => {
  if (!storageUrl || storageUrl === "purged://") return false;

  if (storageUrl.startsWith("local://")) {
    return deleteLocalStorage(storageUrl);
  }

  const key = extractStorageKey(storageUrl);
  if (!key) {
    console.warn("[STORAGE] Skipping delete — could not extract key:", storageUrl);
    return false;
  }

  return deleteObject(key);
};

export const getDownloadUrl = async (storageUrl, expiresIn = 900) => {
  if (storageUrl.startsWith("local://")) {
    return storageUrl;
  }

  const key = extractStorageKey(storageUrl);
  if (!key) {
    console.error("[STORAGE] Cannot extract key from storage URL:", storageUrl);
    throw new Error("Invalid storage URL format");
  }

  console.log("[STORAGE] Extracted key:", key);
  const command = new GetObjectCommand({
    Bucket: env.R2_BUCKET,
    Key: key,
  });

  const signedUrl = await getSignedUrl(s3, command, { expiresIn });
  console.log("[STORAGE] Generated signed URL:", signedUrl);
  return signedUrl;
};
