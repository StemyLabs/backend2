"""Offline stereo correlation / width helpers."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import numpy as np
import soundfile as sf


def analyze_stereo(path: Path) -> dict[str, Any]:
    audio, _sr = sf.read(str(path), always_2d=True)
    if audio.shape[1] != 2:
        raise ValueError("Stereo required")
    left = audio[:, 0]
    right = audio[:, 1]
    corr = float(np.corrcoef(left, right)[0, 1]) if left.size > 1 else 1.0
    mid = 0.5 * (left + right)
    side = 0.5 * (left - right)
    mid_e = float(np.mean(mid**2))
    side_e = float(np.mean(side**2))
    width = side_e / (mid_e + side_e) if (mid_e + side_e) > 0 else 0.0
    return {
        "stereo_correlation": corr,
        "mid_energy": mid_e,
        "side_energy": side_e,
        "stereo_width_provisional": width,
    }
