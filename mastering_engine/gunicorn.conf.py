# gunicorn.conf.py – production Gunicorn settings for the mastering engine
import os
import sys

# This file lives in mastering_engine/ — keep imports working from any cwd
_engine_dir = os.path.dirname(os.path.abspath(__file__))
if _engine_dir not in sys.path:
    sys.path.insert(0, _engine_dir)

# Bind address - Render sets PORT env var
port = os.environ.get("PORT")
bind = f"0.0.0.0:{port}" if port else "0.0.0.0:10000"

# Two workers — Render Standard (2 GB) handles 2 concurrent masters at ~1 GB peak
workers = int(os.environ.get("WEB_CONCURRENCY", "2"))

# Worker class
worker_class = "sync"

# Long tracks (30+ min) can take several minutes to process
timeout = int(os.environ.get("GUNICORN_TIMEOUT", "1200"))

# Graceful timeout for shutdown
graceful_timeout = 30

# Keepalive to prevent connection drops
keepalive = 5

# Max requests per worker before restart (disable for stability)
max_requests = 0
max_requests_jitter = 0

# Logging
accesslog = "-"
errorlog = "-"
loglevel = "info"

# Disable preload to save memory on 2 GB instances
preload_app = False