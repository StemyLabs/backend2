#!/usr/bin/env python3
"""
Smoke test for streaming mastering (run on Render or any host with requirements installed).

  python validate_streaming.py [input.wav]

Generates a 5 s sine tone if no input is provided.
"""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

from dsp_chain import master_audio, master_audio_file


def main() -> int:
    if len(sys.argv) > 1:
        src = Path(sys.argv[1])
        if not src.is_file():
            print(f"Missing file: {src}")
            return 1
    else:
        sr = 44_100
        t = np.linspace(0, 5, sr * 5, dtype=np.float32)
        tone = 0.25 * np.sin(2 * np.pi * 440 * t)
        stereo = np.stack([tone, tone], axis=1)
        src = Path(tempfile.gettempdir()) / "stemy_validate_in.wav"
        sf.write(src, stereo, sr)
        print(f"Generated test input: {src}")

    out_file = src.with_name(src.stem + "_stream_mastered.wav")
    out_bytes = src.with_name(src.stem + "_bytes_mastered.wav")

    a_file = master_audio_file(src, out_file, "pop")
    raw = src.read_bytes()
    wav_bytes, a_bytes = master_audio(raw, "pop")
    out_bytes.write_bytes(wav_bytes)

    print("master_audio_file:", a_file)
    print("master_audio:     ", a_bytes)
    print("Outputs:", out_file, out_bytes)

    lufs_delta = abs(a_file["lufs"] - a_bytes["lufs"])
    if lufs_delta > 0.3:
        print(f"WARN: LUFS delta between paths = {lufs_delta:.2f} LU")
        return 2
    print("OK — file and bytes APIs agree within 0.3 LU")
    return 0


if __name__ == "__main__":
    sys.exit(main())
