"""
dsp_chain.py – Server-side mastering DSP using pedalboard + pyloudnorm.

Streaming / disk-backed pipeline for low RAM (Render 2 GB):
  Pass 1: chunked EQ → saturation → widen → pre_lufs.wav
  LUFS measure via memmap (BS.1770 integrated)
  Pass 2: chunked gain → limiter → ceiling → 24-bit PCM WAV
"""

from __future__ import annotations

import gc
import io
import logging
import os
import struct
import time
import warnings
from pathlib import Path

import numpy as np
import soundfile as sf
import pyloudnorm as pyln

warnings.filterwarnings("ignore", category=UserWarning, module="pedalboard")

try:
    from pedalboard import (
        Pedalboard,
        HighpassFilter,
        LowShelfFilter,
        PeakFilter,
        HighShelfFilter,
        Compressor,
        Limiter,
        Resample,
    )
    PEDALBOARD_AVAILABLE = True
except ImportError as e:
    PEDALBOARD_AVAILABLE = False
    _PEDALBOARD_ERROR = str(e)

from genres import get_preset, DEFAULT_GENRE
from io_stream import (
    CHUNK_SECONDS,
    TARGET_SR as IO_TARGET_SR,
    check_disk_space,
    channels_first_to_samples,
    ensure_temp_dir,
    iter_pedalboard_chunks,
    iter_soundfile_chunks,
    mmap_stereo_wav,
    normalize_to_stereo_channels_first,
    open_pedalboard_input,
    open_pre_lufs_writer,
    probe_audio_path,
    safe_unlink,
    samples_to_channels_first,
    temp_path,
    write_bytes_to_temp,
)

log = logging.getLogger(__name__)

TARGET_SR = IO_TARGET_SR
TARGET_BITS = 24
TARGET_LUFS = -14.0
TARGET_TP_DB = -1.0
MAX_GAIN_DB = 30.0

MAX_UPLOAD_BYTES = int(
    os.environ.get("STEMY_MAX_UPLOAD_BYTES", str(100 * 1024 * 1024))
)


# ─────────────────────────── helpers ────────────────────────────────────────

def _guess_image_mime(data: bytes) -> str:
    if len(data) < 4:
        return "image/jpeg"
    if data[:4] == b"\x89PNG":
        return "image/png"
    return "image/jpeg"


def _db_to_lin(db: float) -> float:
    return 10.0 ** (db / 20.0)


def _lin_to_db(lin: float) -> float:
    if lin <= 0:
        return -120.0
    return 20.0 * np.log10(lin)


def _probe_audio(input_bytes: bytes) -> tuple[float, int, int]:
    with io.BytesIO(input_bytes) as buf:
        with sf.SoundFile(buf) as f:
            frames = len(f)
            sr = int(f.samplerate)
            ch = int(f.channels)
    if sr <= 0 or frames <= 0:
        raise ValueError("Could not read audio length from file.")
    return frames / sr, sr, ch


def _check_upload_size(size_bytes: int) -> None:
    file_mb = size_bytes / (1024 * 1024)
    max_mb = MAX_UPLOAD_BYTES / (1024 * 1024)
    if size_bytes > MAX_UPLOAD_BYTES:
        raise ValueError(
            f"File is {file_mb:.1f} MB; maximum upload size is {max_mb:.0f} MB."
        )


def _ensure_stereo(audio: np.ndarray) -> np.ndarray:
    if audio.ndim == 1:
        audio = np.stack([audio, audio], axis=-1)
    elif audio.shape[1] == 1:
        audio = np.concatenate([audio, audio], axis=-1)
    elif audio.shape[1] > 2:
        audio = audio[:, :2]
    return audio.astype(np.float32)


def _build_eq_board(preset: dict) -> tuple[Pedalboard, dict]:
    comp_cfg = preset["comp"]
    board = Pedalboard([
        HighpassFilter(cutoff_frequency_hz=preset["hpf_hz"]),
        LowShelfFilter(
            cutoff_frequency_hz=preset["low_shelf"]["freq_hz"],
            gain_db=preset["low_shelf"]["gain_db"],
        ),
        PeakFilter(
            cutoff_frequency_hz=preset["mid_dip"]["freq_hz"],
            gain_db=preset["mid_dip"]["gain_db"],
            q=preset["mid_dip"]["q"],
        ),
        PeakFilter(
            cutoff_frequency_hz=preset["presence"]["freq_hz"],
            gain_db=preset["presence"]["gain_db"],
            q=preset["presence"]["q"],
        ),
        HighShelfFilter(
            cutoff_frequency_hz=preset["air_shelf"]["freq_hz"],
            gain_db=preset["air_shelf"]["gain_db"],
        ),
        Compressor(
            threshold_db=comp_cfg["threshold_db"],
            ratio=comp_cfg["ratio"],
            attack_ms=comp_cfg["attack_ms"],
            release_ms=comp_cfg["release_ms"],
        ),
    ])
    return board, comp_cfg


def _process_board_chunk(
    board: Pedalboard,
    chunk_cf: np.ndarray,
    sr: int,
    *,
    reset: bool,
) -> np.ndarray:
    chunk_cf = normalize_to_stereo_channels_first(chunk_cf)
    out = board.process(chunk_cf, sample_rate=sr, reset=reset)
    return np.asarray(out, dtype=np.float32)


def _flush_board(board: Pedalboard, sr: int) -> np.ndarray | None:
    """Drain plugin tails after chunked processing."""
    try:
        tail = board.process(
            np.zeros((2, 1), dtype=np.float32),
            sample_rate=sr,
            reset=True,
        )
        if tail is not None and np.asarray(tail).size > 0:
            return np.asarray(tail, dtype=np.float32)
    except Exception:
        pass
    return None


def _normalize_input_to_44k(input_path: Path) -> Path:
    """Fallback: chunked soundfile read + resample to 44.1 kHz stereo float WAV."""
    out = temp_path("_norm.wav")
    _, src_sr, _ = probe_audio_path(input_path)
    resample_board = Pedalboard([Resample(target_sample_rate=TARGET_SR)])
    block = max(1, CHUNK_SECONDS * src_sr)
    with sf.SoundFile(str(input_path)) as inf:
        with open_pre_lufs_writer(out) as outf:
            while inf.tell() < inf.frames:
                data = inf.read(block, dtype="float32", always_2d=True)
                if data is None or len(data) == 0:
                    break
                data = _ensure_stereo(data)
                if src_sr != TARGET_SR:
                    data = _process_board_chunk(
                        resample_board, data.T, src_sr, reset=False
                    ).T
                outf.write(data)
    return out


def _pass1_stream(
    input_path: Path,
    pre_lufs_path: Path,
    preset: dict,
) -> float:
    """Stream input → pre_lufs float32 WAV. Returns input duration in seconds."""
    board, comp_cfg = _build_eq_board(preset)
    makeup = _db_to_lin(comp_cfg.get("makeup_db", 0.0))
    sat_drive = preset["saturation_drive"]
    width = preset.get("width", 1.0)

    normalized_path: Path | None = None
    try:
        audio_in = None
        try:
            audio_in, sr = open_pedalboard_input(input_path)
            chunk_iter = iter_pedalboard_chunks(audio_in, sr)
            input_dur = audio_in.frames / sr
            log.info("Input via AudioFile @ %d Hz, %.2f s", sr, input_dur)
        except Exception as exc:
            log.warning("AudioFile open failed (%s), using soundfile fallback", exc)
            normalized_path = _normalize_input_to_44k(input_path)
            sr = TARGET_SR
            _, _, _ = probe_audio_path(normalized_path)
            with sf.SoundFile(str(normalized_path)) as nf:
                input_dur = len(nf) / sr
            chunk_iter = (
                samples_to_channels_first(_ensure_stereo(c))
                for c in iter_soundfile_chunks(normalized_path, sr)
            )

        with open_pre_lufs_writer(pre_lufs_path) as writer:
            first = True
            for chunk_cf in chunk_iter:
                chunk_cf = normalize_to_stereo_channels_first(chunk_cf)
                effected = _process_board_chunk(
                    board, chunk_cf, sr, reset=first
                )
                first = False
                effected *= makeup
                samples = channels_first_to_samples(effected)
                samples = _soft_clip_saturation(samples, sat_drive)
                samples = _ms_widen(samples, width)
                writer.write(samples)

            tail = _flush_board(board, sr)
            if tail is not None and tail.size > 0:
                tail = normalize_to_stereo_channels_first(tail) * makeup
                samples = _ms_widen(
                    _soft_clip_saturation(channels_first_to_samples(tail), sat_drive),
                    width,
                )
                writer.write(samples)

        return input_dur
    finally:
        if audio_in is not None:
            try:
                audio_in.close()
            except Exception:
                pass
        safe_unlink(normalized_path)


def _pass2_stream(
    pre_lufs_path: Path,
    output_path: Path,
    preset: dict,
    *,
    measured_lufs: float,
    gain_needed: float,
    target_tp_db: float,
) -> tuple[float, float, float, float]:
    """Apply LUFS gain, limiter, ceiling; write PCM_24 WAV. Return (lufs, tp, dr, duration)."""
    limiter = Pedalboard([
        Limiter(
            threshold_db=preset["limiter"]["threshold_db"],
            release_ms=preset["limiter"]["release_ms"],
        )
    ])
    gain_lin = _db_to_lin(gain_needed)
    hard_ceil = _db_to_lin(target_tp_db - 0.05)

    sample_count = 0
    peak_max = 0.0
    rms_sum = 0.0
    first = True

    with sf.SoundFile(
        str(output_path),
        mode="w",
        samplerate=TARGET_SR,
        channels=2,
        format="WAV",
        subtype="PCM_24",
    ) as pcm_writer:
        for chunk in iter_soundfile_chunks(pre_lufs_path, TARGET_SR):
            chunk = chunk * gain_lin
            chunk_cf = samples_to_channels_first(chunk)
            chunk_cf = _process_board_chunk(
                limiter, chunk_cf, TARGET_SR, reset=first
            )
            first = False
            chunk = channels_first_to_samples(chunk_cf)

            peak = float(np.max(np.abs(chunk)))
            peak_max = max(peak_max, peak)
            if peak > hard_ceil:
                chunk = chunk * (hard_ceil / peak)
                peak_max = min(peak_max, hard_ceil)

            rms_sum += float(np.mean(chunk ** 2)) * len(chunk)
            sample_count += len(chunk)
            pcm_writer.write(chunk)

        tail = _flush_board(limiter, TARGET_SR)
        if tail is not None and tail.size > 0:
            chunk = channels_first_to_samples(tail)
            peak = float(np.max(np.abs(chunk)))
            peak_max = max(peak_max, peak)
            if peak > hard_ceil:
                chunk = chunk * (hard_ceil / peak)
            rms_sum += float(np.mean(chunk ** 2)) * len(chunk)
            sample_count += len(chunk)
            pcm_writer.write(chunk)

    duration = sample_count / TARGET_SR if TARGET_SR else 0.0
    final_tp = _lin_to_db(peak_max) if peak_max > 1e-6 else -120.0
    rms = (rms_sum / max(sample_count, 1)) ** 0.5
    rms_db = 20 * np.log10(rms) if rms > 1e-6 else -120.0
    final_dr = max(0.0, final_tp - rms_db)
    if np.isfinite(measured_lufs) and measured_lufs > -70:
        final_lufs = float(np.clip(measured_lufs + gain_needed, -70.0, 0.0))
    else:
        final_lufs = -14.0

    return final_lufs, final_tp, final_dr, duration


# ─────────────────────────── metadata embedding ─────────────────────────────

def _embed_riff_metadata(
    wav_bytes: bytes,
    metadata: dict | None = None,
    artwork_bytes: bytes | None = None,
) -> bytes:
    if (not metadata and not artwork_bytes) or len(wav_bytes) < 12:
        return wav_bytes
    if wav_bytes[:4] != b"RIFF" or wav_bytes[8:12] != b"WAVE":
        return wav_bytes
    try:
        info_payload = b""
        if metadata:
            chunk_map = {
                "INAM": metadata.get("title"),
                "IART": metadata.get("artist"),
                "IPRD": metadata.get("album"),
                "ICRD": metadata.get("year"),
                "IGNR": metadata.get("genre"),
                "ICOP": metadata.get("copyright"),
                "ISRC": metadata.get("isrc"),
                "ITRK": metadata.get("track"),
                "ICMW": metadata.get("composer"),
                "ICMT": metadata.get("comment"),
            }
            for ck_id, val in chunk_map.items():
                val_str = str(val).strip() if val else ""
                if not val_str:
                    continue
                raw = val_str.encode("utf-8") + b"\x00"
                if len(raw) % 2:
                    raw += b"\x00"
                info_payload += ck_id.encode() + struct.pack("<I", len(raw)) + raw

        chunks_to_insert = []
        if info_payload:
            list_body = b"INFO" + info_payload
            if len(list_body) % 2:
                list_body += b"\x00"
            chunks_to_insert.append(
                b"LIST" + struct.pack("<I", len(list_body)) + list_body
            )
        if artwork_bytes:
            mime_type = _guess_image_mime(artwork_bytes)
            mime_bytes = mime_type.encode("utf-8") + b"\x00"
            if len(mime_bytes) % 2:
                mime_bytes += b"\x00"
            pict_data = mime_bytes + artwork_bytes
            if len(pict_data) % 2:
                pict_data += b"\x00"
            chunks_to_insert.append(
                b"PICT" + struct.pack("<I", len(pict_data)) + pict_data
            )
        if not chunks_to_insert:
            return wav_bytes

        pos = 12
        out = bytearray(wav_bytes[:12])
        while pos + 8 <= len(wav_bytes):
            ck_id = wav_bytes[pos : pos + 4]
            ck_sz = struct.unpack("<I", wav_bytes[pos + 4 : pos + 8])[0]
            ck_end = pos + 8 + ck_sz
            if ck_sz % 2:
                ck_end += 1
            if ck_id == b"data":
                for chunk_bytes in chunks_to_insert:
                    out.extend(chunk_bytes)
            out.extend(wav_bytes[pos:ck_end])
            pos = ck_end
        riff_size = len(out) - 8
        out[4:8] = struct.pack("<I", riff_size)
        return bytes(out)
    except Exception as exc:
        log.warning("Failed to embed RIFF metadata: %s", exc)
        return wav_bytes


def _embed_riff_metadata_file(
    wav_path: Path,
    metadata: dict | None = None,
    artwork_bytes: bytes | None = None,
) -> None:
    """Embed metadata; loads file only when under STEMY_METADATA_MAX_BYTES."""
    if not metadata and not artwork_bytes:
        return
    max_embed = int(os.environ.get("STEMY_METADATA_MAX_BYTES", str(200 * 1024 * 1024)))
    size = wav_path.stat().st_size
    if size > max_embed:
        log.warning(
            "Skipping RIFF metadata embed for %.1f MB output (limit %.1f MB)",
            size / 1e6,
            max_embed / 1e6,
        )
        return
    data = wav_path.read_bytes()
    embedded = _embed_riff_metadata(data, metadata, artwork_bytes)
    if embedded is not data:
        wav_path.write_bytes(embedded)
    del data


def _true_peak_db(audio: np.ndarray) -> float:
    peak = float(np.max(np.abs(audio)))
    if peak < 0.001:
        return -120.0
    if audio.ndim == 1:
        return _lin_to_db(peak)
    interp = np.max(np.abs(0.5 * (audio[:-1] + audio[1:])))
    return _lin_to_db(max(peak, float(np.max(np.abs(interp)))))


def _lufs(audio: np.ndarray, sr: int) -> float:
    meter = pyln.Meter(sr)
    if isinstance(audio, np.memmap):
        return meter.integrated_loudness(audio)
    return meter.integrated_loudness(np.asarray(audio, dtype=np.float64))


def _dynamic_range(audio: np.ndarray) -> float:
    rms = np.sqrt(np.mean(audio ** 2))
    if rms < 1e-6:
        return 0.0
    rms_db = 20 * np.log10(rms)
    peak_db = _true_peak_db(audio)
    return max(0, peak_db - rms_db)


def _ms_widen(audio: np.ndarray, width: float) -> np.ndarray:
    if width <= 0 or abs(width - 1.0) < 1e-3:
        return audio
    L = audio[:, 0]
    R = audio[:, 1]
    mid = (L + R) * 0.5
    side = (L - R) * 0.5
    side_scaled = side * width
    L_out = mid + side_scaled
    R_out = mid - side_scaled
    out = np.stack([L_out, R_out], axis=-1)
    rms_in = float(np.sqrt(np.mean(audio ** 2))) + 1e-9
    rms_out = float(np.sqrt(np.mean(out ** 2))) + 1e-9
    return (out * (rms_in / rms_out)).astype(np.float32)


def _soft_clip_saturation(audio: np.ndarray, drive: float) -> np.ndarray:
    k = float(np.clip(drive, 0.0, 1.5))
    if k < 1e-4:
        return audio
    pre = audio * (1.0 + k * 0.5)
    shaped = pre - (k / 3.0) * pre ** 3
    normaliser = float(np.tanh(1.1))
    out = np.tanh(shaped * 1.1) / normaliser
    rms_in = float(np.sqrt(np.mean(audio ** 2))) + 1e-9
    rms_out = float(np.sqrt(np.mean(out ** 2))) + 1e-9
    return (out * (rms_in / rms_out)).astype(np.float32)


# ─────────────────────────── public API ─────────────────────────────────────

def master_audio_file(
    input_path: Path | str,
    output_path: Path | str,
    genre: str = DEFAULT_GENRE,
    *,
    target_lufs: float = TARGET_LUFS,
    target_tp_db: float = TARGET_TP_DB,
    metadata: dict | None = None,
    artwork_bytes: bytes | None = None,
) -> dict:
    """
    Master from disk paths; writes 24-bit PCM WAV to output_path.
    Returns analysis dict (same keys as master_audio).
    """
    if not PEDALBOARD_AVAILABLE:
        raise RuntimeError(
            f"pedalboard is not installed: {_PEDALBOARD_ERROR}. "
            "Run: pip install pedalboard"
        )

    input_path = Path(input_path)
    output_path = Path(output_path)
    ensure_temp_dir()

    t0 = time.perf_counter()
    size_bytes = input_path.stat().st_size
    _check_upload_size(size_bytes)

    duration_sec, src_sr, channels = probe_audio_path(input_path)
    check_disk_space(duration_sec)

    preset = get_preset(genre)
    target_lufs = preset.get("target_lufs", target_lufs)
    target_tp_db = preset.get("target_tp_db", target_tp_db)

    log.info(
        "Mastering — genre=%s target_lufs=%.1f target_tp=%.1f (%.2f s, %d Hz, %d ch, %.1f MB)",
        genre,
        target_lufs,
        target_tp_db,
        duration_sec,
        src_sr,
        channels,
        size_bytes / (1024 * 1024),
    )

    pre_lufs_path = temp_path("_pre.wav")
    try:
        t1 = time.perf_counter()
        input_dur = _pass1_stream(input_path, pre_lufs_path, preset)
        log.info("Pass 1 (DSP): %.1fs", time.perf_counter() - t1)

        gc.collect()

        t2 = time.perf_counter()
        audio_mmap = mmap_stereo_wav(pre_lufs_path, subtype="FLOAT")
        try:
            measured_lufs = _lufs(audio_mmap, TARGET_SR)
        finally:
            del audio_mmap
            gc.collect()
        gain_needed = 0.0
        if np.isfinite(measured_lufs) and measured_lufs > -70:
            gain_needed = float(
                np.clip(target_lufs - measured_lufs, -MAX_GAIN_DB, MAX_GAIN_DB)
            )
        log.info("LUFS measure: %.2f → gain %.2f dB (%.1fs)", measured_lufs, gain_needed, time.perf_counter() - t2)

        t3 = time.perf_counter()
        final_lufs, final_tp, final_dr, duration = _pass2_stream(
            pre_lufs_path,
            output_path,
            preset,
            measured_lufs=measured_lufs,
            gain_needed=gain_needed,
            target_tp_db=target_tp_db,
        )
        log.info("Pass 2 (gain/limit/out): %.1fs", time.perf_counter() - t3)

        if metadata or artwork_bytes:
            _embed_riff_metadata_file(output_path, metadata, artwork_bytes)

        total = time.perf_counter() - t0
        log.info(
            "TOTAL: %.1fs (rtf=%.1fx) LUFS=%.1f dBTP=%.2f DR=%.1f",
            total,
            total / max(input_dur, 0.01),
            final_lufs,
            final_tp,
            final_dr,
        )

        return {
            "lufs": round(final_lufs, 1),
            "dbtp": round(final_tp, 2),
            "dr": round(final_dr, 1),
            "duration": round(duration, 2),
            "genre": genre,
            "target_lufs": target_lufs,
            "target_tp_db": target_tp_db,
        }
    finally:
        safe_unlink(pre_lufs_path)
        gc.collect()


def master_audio(
    input_bytes: bytes,
    genre: str = DEFAULT_GENRE,
    *,
    target_lufs: float = TARGET_LUFS,
    target_tp_db: float = TARGET_TP_DB,
    metadata: dict | None = None,
    artwork_bytes: bytes | None = None,
) -> tuple[bytes, dict]:
    """In-memory wrapper: temp files + master_audio_file."""
    in_path = write_bytes_to_temp(input_bytes, suffix=".audio")
    out_path = temp_path("_out.wav")
    try:
        analysis = master_audio_file(
            in_path,
            out_path,
            genre,
            target_lufs=target_lufs,
            target_tp_db=target_tp_db,
            metadata=metadata,
            artwork_bytes=artwork_bytes,
        )
        return out_path.read_bytes(), analysis
    finally:
        safe_unlink(in_path)
        safe_unlink(out_path)


if __name__ == "__main__":
    import sys

    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")

    if len(sys.argv) < 2:
        print("Usage: python dsp_chain.py <input_audio> [genre] [output.wav]")
        sys.exit(1)

    src = Path(sys.argv[1])
    genre = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_GENRE
    dest = Path(sys.argv[3]) if len(sys.argv) > 3 else src.with_stem(src.stem + "_mastered")

    if src.exists() and src.stat().st_size <= MAX_UPLOAD_BYTES:
        analysis = master_audio_file(src, dest, genre)
    else:
        raw = src.read_bytes()
        out, analysis = master_audio(raw, genre)
        dest.write_bytes(out)

    print(f"✓ Mastered → {dest}")
    print(
        f"  LUFS={analysis['lufs']} | dBTP={analysis['dbtp']} | "
        f"DR={analysis['dr']} dB | {analysis['duration']}s"
    )
