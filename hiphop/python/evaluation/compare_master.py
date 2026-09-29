#!/usr/bin/env python3
"""Compare pre/post master analysis summaries (offline evaluation)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "analysis"))

from loudness_analysis import analyze_loudness
from stereo_analysis import analyze_stereo


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    before = {"loudness": analyze_loudness(args.input), "stereo": analyze_stereo(args.input)}
    after = {"loudness": analyze_loudness(args.output), "stereo": analyze_stereo(args.output)}
    print(json.dumps({"before": before, "after": after}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
