"""Offline spectral band energy helpers."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import numpy as np
import soundfile as sf
from scipy.signal import stft


def analyze_spectrum(path: Path) -> dict[str, Any]:
    audio, sr = sf.read(str(path), always_2d=True)
    mono = audio.mean(axis=1)
    _, _, zxx = stft(mono, fs=sr, nperseg=2048)
    power = np.mean(np.abs(zxx) ** 2, axis=1)
    freqs = np.fft.rfftfreq(2048, d=1.0 / sr)

    def band(lo: float, hi: float) -> float:
        mask = (freqs >= lo) & (freqs < hi)
        return float(np.sum(power[mask])) if np.any(mask) else 0.0

    return {
        "sub_bass": band(0, 60),
        "bass": band(60, 250),
        "low_mid": band(250, 500),
        "mid": band(500, 2000),
        "high_mid": band(2000, 6000),
        "high": band(6000, sr / 2),
    }
