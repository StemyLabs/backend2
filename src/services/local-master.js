import { spawn } from "node:child_process";
import fsp from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { env } from "../config/env.js";
import { getMasterTmpDir } from "../utils/master-temp.js";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const defaultCli = path.resolve(__dirname, "../../mastering_engine/cli_master.py");

const outExt = () => (process.env.STEMY_OUTPUT_EXT || ".flac").toLowerCase();

const resolveOutputPath = (outputPath) => {
  const ext = outExt();
  return outputPath.endsWith(ext)
    ? outputPath
    : `${outputPath.replace(/\.[^.]+$/, "")}${ext}`;
};

/** HTTP timeout for /master/local — must exceed ffmpeg time for long tracks. */
export const pythonHttpTimeoutMs = (fileSizeBytes = 0) => {
  const fixed = parseInt(process.env.STEMY_PYTHON_HTTP_TIMEOUT_MS || "0", 10);
  if (fixed > 0) return fixed;
  const mb = fileSizeBytes / (1024 * 1024);
  // ~30s per MB + 2 min base, cap 15 min (matches Python ffmpeg cap)
  return Math.min(900_000, Math.max(120_000, Math.ceil(mb * 30_000 + 120_000)));
};

/** Warm Gunicorn on localhost — fast (no Python cold start). */
export const runLocalMasterHttp = async ({
  inputPath,
  outputPath,
  genre,
  timeoutMs,
}) => {
  const base = env.PYTHON_ENGINE_URL.replace(/\/$/, "");
  const out = resolveOutputPath(outputPath);
  const ms = timeoutMs ?? pythonHttpTimeoutMs(0);
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), ms);

  let res;
  try {
    res = await fetch(`${base}/master/local`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        path: inputPath,
        output_path: out,
        genre,
      }),
      signal: controller.signal,
    });
  } finally {
    clearTimeout(timeoutId);
  }

  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    throw new Error(data.error || data.description || `Local master HTTP ${res.status}`);
  }

  const written = data.output_path || out;
  if (written !== out && (await fsp.stat(written).catch(() => null))) {
    await fsp.copyFile(written, out).catch(() => {});
  }

  return {
    analysis: {
      ...data,
      elapsed_sec: (data.processing_ms || 0) / 1000,
    },
    outputPath: out,
  };
};

/** Spawn Python CLI — slow (~8s cold start); use only if HTTP unavailable. */
export const runLocalMasterCli = ({ inputPath, outputPath, genre }) =>
  new Promise((resolve, reject) => {
    const python = env.PYTHON_BIN || "python3";
    const cli = env.PYTHON_CLI_PATH || defaultCli;
    const out = resolveOutputPath(outputPath);

    const proc = spawn(
      python,
      [cli, "--input", inputPath, "--output", out, "--genre", genre],
      {
        env: {
          ...process.env,
          STEMY_TEMP_DIR: getMasterTmpDir(),
          STEMY_TURBO: process.env.STEMY_TURBO || "1",
        },
        stdio: ["ignore", "pipe", "pipe"],
      },
    );

    let stdout = "";
    let stderr = "";
    proc.stdout.on("data", (d) => {
      stdout += d.toString();
    });
    proc.stderr.on("data", (d) => {
      stderr += d.toString();
    });

    proc.on("error", reject);
    proc.on("close", (code) => {
      if (code !== 0) {
        reject(new Error(stderr.trim() || `Python CLI exited ${code}`));
        return;
      }
      const lines = stdout.trim().split("\n").filter(Boolean);
      const last = lines[lines.length - 1];
      try {
        const analysis = JSON.parse(last);
        if (analysis.error) {
          reject(new Error(analysis.error));
          return;
        }
        resolve({ analysis, outputPath: analysis.output_path || out });
      } catch {
        reject(new Error(`Invalid CLI JSON: ${last || stdout}`));
      }
    });
  });

/** VPS default: HTTP to warm Gunicorn. Set PYTHON_LOCAL_MODE=cli to force subprocess. */
export const runLocalMaster = async (opts) => {
  if (opts.fileSizeBytes && !opts.timeoutMs) {
    opts = { ...opts, timeoutMs: pythonHttpTimeoutMs(opts.fileSizeBytes) };
  }
  const mode = (process.env.PYTHON_LOCAL_MODE || "http").toLowerCase();
  const isLocalEngine = /localhost|127\.0\.0\.1/i.test(env.PYTHON_ENGINE_URL);

  if (mode === "cli" || !isLocalEngine) {
    return runLocalMasterCli(opts);
  }
  return runLocalMasterHttp(opts);
};
