"""
turbo_chain.py – VPS-oriented mastering targeting ≤90s for files up to 100 MB.

Primary path: ffmpeg filter graph (multi-threaded decode/encode).
Fallback: parallel Pedalboard segment workers when ffmpeg is unavailable.
"""

from __future__ import annotations

import json
import logging
import os
import re
import shutil
import subprocess
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import soundfile as sf

from genres import get_preset
from io_stream import (
    TARGET_SR,
    ensure_temp_dir,
    probe_audio_path,
    safe_unlink,
    temp_path,
)

log = logging.getLogger(__name__)

TARGET_SEC = float(os.environ.get("STEMY_TARGET_SEC", "90"))
TURBO_WORKERS = int(os.environ.get("STEMY_TURBO_WORKERS", str(min(6, os.cpu_count() or 4))))
LUFS_PROBE_SEC = float(os.environ.get("STEMY_LUFS_PROBE_SEC", "90"))
OUTPUT_EXT = os.environ.get("STEMY_OUTPUT_EXT", ".flac").lower()
# Tracks longer than this use parallel workers (fast). Shorter tracks use ffmpeg + loudnorm.
FFMPEG_MAX_DURATION_SEC = float(os.environ.get("STEMY_FFMPEG_MAX_DURATION_SEC", "600"))
PARALLEL_MAX_DURATION_SEC = float(
    os.environ.get("STEMY_PARALLEL_MAX_DURATION_SEC", "7200")
)
FLAC_COMPRESSION = int(os.environ.get("STEMY_FLAC_COMPRESSION", "1"))


def _ffmpeg_timeout_for(duration_sec: float) -> int:
    """Scale ffmpeg wait with track length (0 = auto). Cap 15 min."""
    fixed = int(os.environ.get("STEMY_FFMPEG_TIMEOUT_SEC", "0"))
    if fixed > 0:
        return fixed
    # ~0.25× realtime + 90s headroom (30 min → ~7 min max cap 900s)
    return max(90, min(900, int(duration_sec * 0.25 + 90)))


def _ffmpeg_bin() -> str | None:
    """Resolve ffmpeg binary (gunicorn often has a minimal PATH without /usr/bin)."""
    explicit = os.environ.get("FFMPEG_PATH", "").strip()
    if explicit and Path(explicit).is_file():
        return explicit
    found = shutil.which("ffmpeg")
    if found:
        return found
    for candidate in ("/usr/bin/ffmpeg", "/usr/local/bin/ffmpeg"):
        if Path(candidate).is_file():
            return candidate
    return None


def _preset_to_ffmpeg_af(preset: dict) -> str:
    """Map genre preset to a compact ffmpeg -af chain (single pass)."""
    ls = preset["low_shelf"]
    md = preset["mid_dip"]
    pr = preset["presence"]
    air = preset["air_shelf"]
    comp = preset["comp"]
    lim = preset["limiter"]
    target_lufs = preset.get("target_lufs", -14.0)
    target_tp = preset.get("target_tp_db", -1.0)
    width = float(preset.get("width", 1.0))
    # stereotools side gain ~ (width - 1) * 0.35
    side = max(0.0, min(0.9, (width - 1.0) * 0.35))

    parts = [
        f"highpass=f={preset['hpf_hz']:.0f}",
        f"lowshelf=f={ls['freq_hz']:.0f}:g={ls['gain_db']:.1f}",
        f"equalizer=f={md['freq_hz']:.0f}:width_type=q:width={md['q']:.2f}:g={md['gain_db']:.1f}",
        f"equalizer=f={pr['freq_hz']:.0f}:width_type=q:width={pr['q']:.2f}:g={pr['gain_db']:.1f}",
        f"highshelf=f={air['freq_hz']:.0f}:g={air['gain_db']:.1f}",
        (
            "acompressor="
            f"threshold={comp['threshold_db']:.1f}dB:ratio={comp['ratio']:.1f}:"
            f"attack={comp['attack_ms']:.0f}:release={comp['release_ms']:.0f}"
        ),
    ]
    makeup = float(comp.get("makeup_db", 0.0))
    if abs(makeup) > 0.05:
        parts.append(f"volume={makeup:.1f}dB")
    if side > 0.02:
        parts.append(f"extrastereo=m={side:.3f}")
    parts.append(
        f"alimiter=limit={lim['threshold_db']:.1f}dB:attack=1:release={lim['release_ms']:.0f}"
    )
    parts.append(
        "loudnorm="
        f"I={target_lufs:.1f}:TP={target_tp:.1f}:LRA=11:linear=true:print_format=json"
    )
    return ",".join(parts)


def _parse_loudnorm_json(stderr: str) -> tuple[float, float]:
    """Extract measured/output LUFS and TP from ffmpeg loudnorm JSON block."""
    lufs = -14.0
    tp = -1.0
    for block in re.findall(r"\{[^{}]*\"input_i\"[^{}]*\}", stderr, re.DOTALL):
        try:
            data = json.loads(block)
            lufs = float(data.get("output_i", data.get("input_i", lufs)))
            tp = float(data.get("output_tp", data.get("input_tp", tp)))
        except (json.JSONDecodeError, TypeError, ValueError):
            continue
    return lufs, tp


def _master_ffmpeg(
    input_path: Path,
    output_path: Path,
    preset: dict,
    *,
    timeout_sec: int | None = None,
) -> dict:
    ffmpeg = _ffmpeg_bin()
    if not ffmpeg:
        raise RuntimeError("ffmpeg not found")

    af = _preset_to_ffmpeg_af(preset)
    out = Path(output_path)
    if out.suffix.lower() != OUTPUT_EXT:
        out = out.with_suffix(OUTPUT_EXT)

    codec = (
        ["-c:a", "flac", "-compression_level", str(FLAC_COMPRESSION)]
        if OUTPUT_EXT == ".flac"
        else ["-c:a", "pcm_s24le"]
    )

    cmd = [
        ffmpeg,
        "-hide_banner",
        "-loglevel",
        "info",
        "-threads",
        "0",
        "-y",
        "-i",
        str(input_path),
        "-af",
        af,
        "-ar",
        str(TARGET_SR),
        "-ac",
        "2",
        *codec,
        str(out),
    ]
    if timeout_sec is None:
        duration_in, _, _ = probe_audio_path(input_path)
        timeout_sec = _ffmpeg_timeout_for(duration_in)
    log.info("Turbo ffmpeg (timeout %ds): %s", timeout_sec, " ".join(cmd[:12]) + " ...")

    t0 = time.perf_counter()
    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=timeout_sec,
        check=False,
    )
    elapsed = time.perf_counter() - t0
    stderr = (proc.stderr or "") + (proc.stdout or "")

    if proc.returncode != 0:
        tail = stderr[-2000:] if stderr else "(no stderr)"
        raise RuntimeError(f"ffmpeg failed ({proc.returncode}): {tail}")

    duration_sec, _, _ = probe_audio_path(out)
    lufs, tp = _parse_loudnorm_json(stderr)
    log.info("Turbo ffmpeg done in %.1fs (target %.0fs)", elapsed, TARGET_SEC)

    return {
        "lufs": round(lufs, 1),
        "dbtp": round(tp, 2),
        "dr": 6.0,
        "duration": round(duration_sec, 2),
        "quality": "turbo",
        "output_format": "flac" if OUTPUT_EXT == ".flac" else "wav",
        "output_path": str(out),
        "engine": "ffmpeg",
        "elapsed_sec": round(elapsed, 2),
    }


@dataclass(frozen=True)
class _SegJob:
    input_path: str
    genre: str
    start_sec: float
    end_sec: float
    out_path: str
    gain_db: float
    target_tp_db: float


def _process_segment(job: _SegJob) -> str:
    """Worker: one time-range, single-pass pedalboard → segment WAV."""
    from dsp_chain import (  # noqa: WPS433 — worker import
        _apply_chain_to_chunk,
        _build_eq_board,
        _db_to_lin,
        _process_board_chunk,
        channels_first_to_samples,
        open_pedalboard_input,
    )
    from io_stream import (
        normalize_to_stereo_channels_first,
        samples_to_channels_first,
    )

    preset = get_preset(job.genre)
    board, comp_cfg = _build_eq_board(preset)
    from pedalboard import Limiter, Pedalboard

    limiter = Pedalboard([
        Limiter(
            threshold_db=preset["limiter"]["threshold_db"],
            release_ms=preset["limiter"]["release_ms"],
        )
    ])
    gain_lin = _db_to_lin(job.gain_db)
    hard_ceil = _db_to_lin(job.target_tp_db - 0.05)

    inp = Path(job.input_path)
    out = Path(job.out_path)
    sr = TARGET_SR
    start_f = int(job.start_sec * sr)
    end_f = int(job.end_sec * sr)

    with sf.SoundFile(str(out), "w", samplerate=sr, channels=2, subtype="FLOAT") as wr:
        audio_in, file_sr = open_pedalboard_input(inp)
        try:
            if start_f > 0:
                audio_in.seek(start_f)
            first = True
            lim_first = True
            while audio_in.tell() < min(end_f, audio_in.frames):
                block = min(262144, min(end_f, audio_in.frames) - audio_in.tell())
                chunk = audio_in.read(block)
                if chunk is None or chunk.size == 0:
                    break
                chunk_cf = normalize_to_stereo_channels_first(np.asarray(chunk, dtype=np.float32))
                samples = _apply_chain_to_chunk(
                    chunk_cf, board, preset, comp_cfg, file_sr, first=first
                )
                first = False
                samples *= gain_lin
                chunk_cf = samples_to_channels_first(samples)
                chunk_cf = _process_board_chunk(limiter, chunk_cf, sr, reset=lim_first)
                lim_first = False
                samples = channels_first_to_samples(chunk_cf)
                peak = float(np.max(np.abs(samples)))
                if peak > hard_ceil:
                    samples = samples * (hard_ceil / peak)
                wr.write(samples)
        finally:
            audio_in.close()
    return str(out)


def _probe_gain_db(input_path: Path, genre: str, target_lufs: float) -> float:
    from dsp_chain import _apply_chain_to_chunk, _build_eq_board, _lufs
    from io_stream import normalize_to_stereo_channels_first, open_pedalboard_input

    preset = get_preset(genre)
    board, comp_cfg = _build_eq_board(preset)
    max_frames = int(LUFS_PROBE_SEC * TARGET_SR)
    parts: list[np.ndarray] = []
    audio_in, sr = open_pedalboard_input(input_path)
    try:
        first = True
        total = 0
        while audio_in.tell() < audio_in.frames and total < max_frames:
            n = min(131072, max_frames - total)
            chunk = audio_in.read(n)
            if chunk is None or chunk.size == 0:
                break
            total += chunk.shape[1]
            chunk_cf = normalize_to_stereo_channels_first(np.asarray(chunk, dtype=np.float32))
            parts.append(
                _apply_chain_to_chunk(chunk_cf, board, preset, comp_cfg, sr, first=first)
            )
            first = False
    finally:
        audio_in.close()

    if not parts:
        return 0.0
    audio = np.concatenate(parts, axis=0)
    measured = _lufs(audio, TARGET_SR)
    if not np.isfinite(measured) or measured <= -70:
        return 0.0
    return float(np.clip(target_lufs - measured, -30.0, 30.0))


def _concat_ffmpeg(segment_paths: list[Path], output_path: Path) -> None:
    ffmpeg = _ffmpeg_bin()
    if not ffmpeg:
        raise RuntimeError("ffmpeg required for concat")
    list_file = temp_path("_concat.txt")
    try:
        lines = [f"file '{p.resolve()}'" for p in segment_paths]
        list_file.write_text("\n".join(lines), encoding="utf-8")
        out = Path(output_path)
        codec = (
            ["-c:a", "flac", "-compression_level", str(FLAC_COMPRESSION)]
            if out.suffix.lower() == ".flac"
            else ["-c:a", "pcm_s24le"]
        )
        cmd = [
            ffmpeg,
            "-hide_banner",
            "-y",
            "-f",
            "concat",
            "-safe",
            "0",
            "-i",
            str(list_file),
            *codec,
            str(out),
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60, check=False)
        if proc.returncode != 0:
            raise RuntimeError((proc.stderr or "")[-1500:])
    finally:
        safe_unlink(list_file)


def _master_parallel(
    input_path: Path,
    output_path: Path,
    genre: str,
    preset: dict,
) -> dict:
    duration_sec, _, _ = probe_audio_path(input_path)
    target_lufs = preset.get("target_lufs", -14.0)
    target_tp = preset.get("target_tp_db", -1.0)

    t0 = time.perf_counter()
    gain_db = _probe_gain_db(input_path, genre, target_lufs)
    log.info("Parallel turbo: %.1fs track, gain %.2f dB, %d workers", duration_sec, gain_db, TURBO_WORKERS)

    n_workers = max(1, min(TURBO_WORKERS, int(duration_sec // 60) + 1))
    seg_len = duration_sec / n_workers
    jobs: list[_SegJob] = []
    seg_paths: list[Path] = []
    for i in range(n_workers):
        start = i * seg_len
        end = duration_sec if i == n_workers - 1 else (i + 1) * seg_len
        seg_out = temp_path(f"_seg{i}.wav")
        seg_paths.append(seg_out)
        jobs.append(
            _SegJob(
                input_path=str(input_path.resolve()),
                genre=genre,
                start_sec=start,
                end_sec=end,
                out_path=str(seg_out),
                gain_db=gain_db,
                target_tp_db=target_tp,
            )
        )

    try:
        with ProcessPoolExecutor(max_workers=n_workers) as pool:
            futs = {pool.submit(_process_segment, j): j for j in jobs}
            for fut in as_completed(futs):
                fut.result()

        out = Path(output_path)
        if out.suffix.lower() != OUTPUT_EXT:
            out = out.with_suffix(OUTPUT_EXT)
        _concat_ffmpeg(seg_paths, out)
    finally:
        for p in seg_paths:
            safe_unlink(p)

    elapsed = time.perf_counter() - t0
    log.info("Parallel turbo done in %.1fs", elapsed)
    return {
        "lufs": round(target_lufs, 1),
        "dbtp": round(target_tp, 2),
        "dr": 6.0,
        "duration": round(duration_sec, 2),
        "quality": "turbo",
        "output_format": "flac" if OUTPUT_EXT == ".flac" else "wav",
        "output_path": str(out),
        "engine": "parallel",
        "elapsed_sec": round(elapsed, 2),
    }


def master_turbo(
    input_path: Path | str,
    output_path: Path | str,
    genre: str,
) -> dict:
    """
    Master with VPS turbo pipeline.

    Short/medium (≤ STEMY_FFMPEG_MAX_DURATION_SEC): ffmpeg + loudnorm (~realtime for short clips).
    Long tracks: parallel Pedalboard segments (avoids loudnorm linear double-pass on full file).
    """
    ensure_temp_dir()
    input_path = Path(input_path)
    output_path = Path(output_path)
    preset = get_preset(genre)
    duration_sec, _, _ = probe_audio_path(input_path)

    if duration_sec > PARALLEL_MAX_DURATION_SEC:
        raise RuntimeError(
            f"Track is {duration_sec / 60:.0f} minutes; max {PARALLEL_MAX_DURATION_SEC / 60:.0f} min."
        )

    if duration_sec > FFMPEG_MAX_DURATION_SEC:
        log.info(
            "Turbo: %.1f min track → parallel workers (ffmpeg loudnorm is slow on long files)",
            duration_sec / 60,
        )
        return _master_parallel(input_path, output_path, genre, preset)

    ffmpeg = _ffmpeg_bin()
    ffmpeg_timeout = _ffmpeg_timeout_for(duration_sec)
    if ffmpeg:
        try:
            return _master_ffmpeg(
                input_path, output_path, preset, timeout_sec=ffmpeg_timeout
            )
        except subprocess.TimeoutExpired:
            log.warning(
                "ffmpeg timed out after %ds for %.1f min track — parallel fallback",
                ffmpeg_timeout,
                duration_sec / 60,
            )
        except Exception as exc:
            log.warning("ffmpeg turbo failed (%s), trying parallel fallback", exc)
    else:
        log.warning(
            "ffmpeg not found on PATH — install ffmpeg for fast turbo (apt install ffmpeg)",
        )

    return _master_parallel(input_path, output_path, genre, preset)
