#!/usr/bin/env python3
"""
Local CLI for VPS co-located Node + Python (no HTTP re-upload).

Usage:
  python cli_master.py --input /path/in.mp3 --output /path/out.flac --genre hiphop
Prints one JSON line to stdout with analysis + output_path.
"""

from __future__ import annotations

import argparse
import json
import logging
import sys
from pathlib import Path

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")

from dsp_chain import master_audio_file  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Stemy local turbo master")
    parser.add_argument("--input", required=True, help="Input audio path")
    parser.add_argument("--output", required=True, help="Output path (.flac or .wav)")
    parser.add_argument("--genre", default="hiphop")
    args = parser.parse_args()

    inp = Path(args.input)
    out = Path(args.output)
    if not inp.is_file():
        print(json.dumps({"error": f"Input not found: {inp}"}))
        return 1

    try:
        analysis = master_audio_file(inp, out, genre=args.genre)
        print(json.dumps(analysis, default=str))
        return 0
    except Exception as exc:
        print(json.dumps({"error": str(exc)}))
        return 1


if __name__ == "__main__":
    sys.exit(main())
