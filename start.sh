#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
PYTHON="${ROOT}/venv/bin/python3"
if [ ! -x "$PYTHON" ]; then
  PYTHON="python3"
fi

# Start Python mastering engine in background (completely detached)
if [ "$NODE_ENV" = "production" ]; then
    echo "Starting Python mastering engine ($PYTHON)..."
    nohup "$PYTHON" -c "
import sys
sys.path.insert(0, '${ROOT}/mastering_engine')
from app import app
app.run(host='127.0.0.1', port=5050, threaded=True)
" > /tmp/python.log 2>&1 &
    
    # Wait for Python to start
    for i in {1..30}; do
        if curl -s http://127.0.0.1:5050/health > /dev/null 2>&1; then
            echo "Python engine ready"
            break
        fi
        sleep 1
    done
fi

# Start Node.js server
exec node src/server.js