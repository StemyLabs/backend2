import test from "node:test";
import assert from "node:assert/strict";
import fs from "fs";
import fsp from "fs/promises";
import os from "os";
import path from "path";
import { spawn } from "child_process";
import ffmpegPath from "ffmpeg-static";
import NodeID3 from "node-id3";
import { embedMp3Metadata } from "../src/services/audio-export.service.js";

const runFfmpeg = (args) =>
  new Promise((resolve, reject) => {
    if (!ffmpegPath) {
      reject(new Error("FFmpeg binary not available"));
      return;
    }
    const proc = spawn(ffmpegPath, args, { windowsHide: true });
    let stderr = "";
    proc.stderr.on("data", (chunk) => {
      stderr += chunk.toString();
    });
    proc.on("error", reject);
    proc.on("close", (code) => {
      if (code === 0) resolve();
      else reject(new Error(stderr.trim() || `FFmpeg exited with code ${code}`));
    });
  });

test("embedMp3Metadata writes ISRC, composer, and copyright", async (t) => {
  if (!ffmpegPath) {
    t.skip("FFmpeg not available");
    return;
  }

  const tmpDir = await fsp.mkdtemp(path.join(os.tmpdir(), "stemy-mp3-meta-"));
  t.after(async () => {
    await fsp.rm(tmpDir, { recursive: true, force: true });
  });

  const mp3Path = path.join(tmpDir, "sample.mp3");
  await runFfmpeg([
    "-y",
    "-f",
    "lavfi",
    "-i",
    "anullsrc=r=44100:cl=stereo",
    "-t",
    "0.2",
    "-codec:a",
    "libmp3lame",
    "-b:a",
    "128k",
    mp3Path,
  ]);
  assert.ok(fs.existsSync(mp3Path));

  await embedMp3Metadata(mp3Path, {
    title: "Test Track",
    artist: "Test Artist",
    isrc: "USXX12345678",
    composer: "Jane Composer",
    copyright: "2026 Test Label",
  });

  const tags = NodeID3.read(mp3Path);
  assert.equal(tags.title, "Test Track");
  assert.equal(tags.artist, "Test Artist");
  assert.equal(tags.ISRC, "USXX12345678");
  assert.equal(tags.composer, "Jane Composer");
  assert.equal(tags.copyright, "2026 Test Label");
});
