"""
io_stream.py – Disk-backed temp files and chunked audio I/O for low-RAM mastering.
"""

from __future__ import annotations

import os
import shutil
import tempfile
import uuid
from contextlib import contextmanager
from pathlib import Path
from typing import Generator, Iterator

import numpy as np
import soundfile as sf

TARGET_SR = 44_100

STEMY_TEMP_DIR = Path(
    os.environ.get("STEMY_TEMP_DIR", os.path.join(tempfile.gettempdir(), "stemy-master"))
)
CHUNK_SECONDS = int(os.environ.get("STEMY_CHUNK_SECONDS", "30"))
MAX_TEMP_BYTES = int(
    os.environ.get("STEMY_MAX_TEMP_BYTES", str(6 * 1024 * 1024 * 1024))
)


def ensure_temp_dir() -> Path:
    STEMY_TEMP_DIR.mkdir(parents=True, exist_ok=True)
    return STEMY_TEMP_DIR


def temp_path(suffix: str = ".wav") -> Path:
    ensure_temp_dir()
    return STEMY_TEMP_DIR / f"{uuid.uuid4().hex}{suffix}"


def write_bytes_to_temp(data: bytes, suffix: str = ".audio") -> Path:
    path = temp_path(suffix)
    path.write_bytes(data)
    return path


def probe_audio_path(path: Path | str) -> tuple[float, int, int]:
    """Return (duration_sec, sample_rate, channels) without decoding full PCM."""
    with sf.SoundFile(str(path)) as f:
        frames = len(f)
        sr = int(f.samplerate)
        ch = int(f.channels)
    if sr <= 0 or frames <= 0:
        raise ValueError("Could not read audio length from file.")
    return frames / sr, sr, ch


def estimate_temp_bytes(duration_sec: float, sr: int = TARGET_SR) -> int:
    """Rough peak temp: pre_lufs float32 + final PCM_24 (~2× PCM + overhead)."""
    pcm_f32 = duration_sec * sr * 2 * 4
    pcm_24 = duration_sec * sr * 2 * 3
    return int(pcm_f32 + pcm_24 + 64 * 1024 * 1024)


def check_disk_space(duration_sec: float) -> None:
    needed = estimate_temp_bytes(duration_sec)
    if needed > MAX_TEMP_BYTES:
        raise ValueError(
            f"Track is too long for available temp storage (estimated {needed / 1e9:.1f} GB). "
            "Try a shorter file or contact support."
        )
    usage = shutil.disk_usage(ensure_temp_dir())
    if usage.free < needed:
        free_gb = usage.free / 1e9
        need_gb = needed / 1e9
        raise ValueError(
            f"Insufficient disk space for mastering (need ~{need_gb:.1f} GB free, have {free_gb:.1f} GB)."
        )


def chunk_frame_count(sr: int = TARGET_SR) -> int:
    return max(1, CHUNK_SECONDS * sr)


@contextmanager
def temp_files(*suffixes: str) -> Generator[list[Path], None, None]:
    paths = [temp_path(s) for s in suffixes]
    try:
        yield paths
    finally:
        for p in paths:
            try:
                p.unlink(missing_ok=True)
            except OSError:
                pass


def safe_unlink(path: Path | str | None) -> None:
    if path is None:
        return
    try:
        Path(path).unlink(missing_ok=True)
    except OSError:
        pass


def open_pedalboard_input(path: Path | str, target_sr: int = TARGET_SR):
    """
    Open input via pedalboard.io.AudioFile, resampled to target_sr when needed.
    Returns (audio_file, sample_rate) or raises on failure.
    """
    from pedalboard.io import AudioFile

    f = AudioFile(str(path))
    if int(f.samplerate) != target_sr:
        f = f.resampled_to(target_sr)
    return f, int(f.samplerate)


def iter_pedalboard_chunks(
    audio_file,
    sr: int,
    seconds: int | None = None,
) -> Iterator[np.ndarray]:
    """Yield (channels, frames) float32 chunks."""
    block = chunk_frame_count(sr) if seconds is None else max(1, int(seconds * sr))
    while audio_file.tell() < audio_file.frames:
        chunk = audio_file.read(block)
        if chunk is None or chunk.size == 0:
            break
        yield np.asarray(chunk, dtype=np.float32)


def normalize_to_stereo_channels_first(chunk: np.ndarray) -> np.ndarray:
    """Return (2, N) float32 for Pedalboard."""
    if chunk.ndim == 1:
        return np.stack([chunk, chunk], axis=0).astype(np.float32)
    if chunk.shape[0] == 1 and chunk.ndim == 2:
        return np.concatenate([chunk, chunk], axis=0).astype(np.float32)
    if chunk.ndim == 2 and chunk.shape[0] > 2:
        return chunk[:2, :].astype(np.float32)
    if chunk.ndim == 2 and chunk.shape[1] == 2 and chunk.shape[0] != 2:
        # soundfile-style (N, 2) accidentally passed
        return chunk.T.astype(np.float32)
    return chunk.astype(np.float32)


def channels_first_to_samples(chunk_cf: np.ndarray) -> np.ndarray:
    """(C, N) → (N, 2) for saturation / widen helpers."""
    if chunk_cf.shape[0] == 1:
        mono = chunk_cf[0]
        return np.stack([mono, mono], axis=-1).astype(np.float32)
    return chunk_cf.T.astype(np.float32)


def samples_to_channels_first(chunk: np.ndarray) -> np.ndarray:
    """(N, 2) → (2, N)."""
    return chunk.T.astype(np.float32)


def open_pre_lufs_writer(path: Path | str, sr: int = TARGET_SR) -> sf.SoundFile:
    return sf.SoundFile(
        str(path),
        mode="w",
        samplerate=sr,
        channels=2,
        format="WAV",
        subtype="FLOAT",
    )


def iter_soundfile_chunks(
    path: Path | str,
    sr: int = TARGET_SR,
    seconds: int | None = None,
) -> Iterator[np.ndarray]:
    """Yield (N, 2) float32 blocks from a WAV file."""
    block = chunk_frame_count(sr) if seconds is None else max(1, int(seconds * sr))
    with sf.SoundFile(str(path)) as f:
        while f.tell() < f.frames:
            data = f.read(block, dtype="float32", always_2d=True)
            if data is None or len(data) == 0:
                break
            if data.shape[1] == 1:
                data = np.concatenate([data, data], axis=1)
            elif data.shape[1] > 2:
                data = data[:, :2]
            yield data.astype(np.float32)


def wav_pcm_data_offset(path: Path | str) -> int:
    """Byte offset of the PCM data chunk in a WAV file."""
    with open(path, "rb") as f:
        header = f.read(12)
        if len(header) < 12 or header[:4] != b"RIFF" or header[8:12] != b"WAVE":
            raise ValueError("Not a valid WAV file")
        while True:
            hdr = f.read(8)
            if len(hdr) < 8:
                raise ValueError("WAV missing data chunk")
            chunk_id, chunk_sz = hdr[:4], int.from_bytes(hdr[4:8], "little")
            if chunk_id == b"data":
                return f.tell()
            f.seek(chunk_sz + (chunk_sz % 2), 1)


def mmap_stereo_wav(path: Path | str, subtype: str = "FLOAT") -> np.memmap:
    """Memory-map stereo WAV PCM as (frames, 2) for pyloudnorm (pages from disk)."""
    with sf.SoundFile(str(path)) as f:
        frames = len(f)
        ch = f.channels
        dtype = np.dtype("float32" if subtype == "FLOAT" else "int16")
    if ch != 2:
        raise ValueError(f"Expected stereo WAV, got {ch} channels")
    offset = wav_pcm_data_offset(path)
    itemsize = dtype.itemsize
    return np.memmap(
        str(path),
        dtype=dtype,
        mode="r",
        offset=offset,
        shape=(frames, 2),
    )
