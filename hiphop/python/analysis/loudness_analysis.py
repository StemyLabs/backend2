"""Offline loudness helpers (Python evaluation only)."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import numpy as np
import soundfile as sf


def analyze_loudness(path: Path) -> dict[str, Any]:
    audio, sr = sf.read(str(path), always_2d=True)
    if audio.shape[1] != 2:
        raise ValueError("Stereo required for M1 analysis utilities")

    peak = float(np.max(np.abs(audio)))
    rms = float(np.sqrt(np.mean(audio**2)))
    crest_db = 20.0 * np.log10(peak / rms) if rms > 0 else 0.0

    # Simple unweighted estimates for offline triage. Prefer C++ BS.1770 for production metrics.
    return {
        "sample_rate": int(sr),
        "duration_seconds": float(audio.shape[0] / sr),
        "sample_peak_dbfs": float(20.0 * np.log10(peak)) if peak > 0 else -120.0,
        "rms_dbfs": float(20.0 * np.log10(rms)) if rms > 0 else -120.0,
        "crest_factor_db": float(crest_db),
        "note": "Python loudness here is unweighted triage; C++ pipeline uses BS.1770",
    }
