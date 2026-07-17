#!/bin/bash
# One-time (or after pull) Python venv setup for VPS / production.
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

echo "==> Installing system packages (libsndfile for audio I/O)..."
apt-get update -qq
apt-get install -y python3-venv python3-full libsndfile1

echo "==> Creating venv at ${ROOT}/venv ..."
python3 -m venv venv

echo "==> Installing Python dependencies..."
venv/bin/pip install --upgrade pip
venv/bin/pip install -r mastering_engine/requirements.txt

echo "==> Verifying mutagen..."
venv/bin/python -c "import mutagen; print('mutagen', mutagen.version_string, 'OK')"

echo ""
echo "Done. Restart your app (e.g. systemctl restart ... or ./start.sh)."
echo "Python binary: ${ROOT}/venv/bin/python3"
