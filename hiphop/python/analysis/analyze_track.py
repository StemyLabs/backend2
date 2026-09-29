#!/usr/bin/env python3
"""Offline track analysis utility (not the production DSP engine)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from loudness_analysis import analyze_loudness
from spectral_analysis import analyze_spectrum
from stereo_analysis import analyze_stereo


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze a stereo audio file")
    parser.add_argument("input", type=Path)
    parser.add_argument("--json", action="store_true", help="Print JSON summary")
    args = parser.parse_args()

    loud = analyze_loudness(args.input)
    spec = analyze_spectrum(args.input)
    stereo = analyze_stereo(args.input)
    summary = {"loudness": loud, "spectral": spec, "stereo": stereo}

    if args.json:
        print(json.dumps(summary, indent=2))
    else:
        for section, data in summary.items():
            print(f"[{section}]")
            for k, v in data.items():
                print(f"  {k}: {v}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
