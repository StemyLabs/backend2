import { spawn } from "node:child_process";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { env } from "../config/env.js";
import { MASTER_TMP_DIR } from "../utils/master-temp.js";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const defaultCli = path.resolve(__dirname, "../../mastering_engine/cli_master.py");

/**
 * Run mastering via local Python CLI (same machine as Node — no HTTP body copy).
 */
export const runLocalMasterCli = ({ inputPath, outputPath, genre }) =>
  new Promise((resolve, reject) => {
    const python = env.PYTHON_BIN || "python3";
    const cli = env.PYTHON_CLI_PATH || defaultCli;
    const outExt = (process.env.STEMY_OUTPUT_EXT || ".flac").toLowerCase();
    const out =
      outputPath.endsWith(outExt) ? outputPath : `${outputPath.replace(/\.[^.]+$/, "")}${outExt}`;

    const proc = spawn(
      python,
      [cli, "--input", inputPath, "--output", out, "--genre", genre],
      {
        env: {
          ...process.env,
          STEMY_TEMP_DIR: MASTER_TMP_DIR,
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
