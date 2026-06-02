#!/bin/bash
set -e

# Start Python mastering engine in background (completely detached)
if [ "$NODE_ENV" = "production" ]; then
    echo "Starting Python mastering engine..."

    # Render runs Node + Python in one container — keep Python memory footprint low.
    export STEMY_DISABLE_NUMBA="${STEMY_DISABLE_NUMBA:-1}"
    export MALLOC_ARENA_MAX=2
    export MAX_MASTER_DURATION_SEC="${MAX_MASTER_DURATION_SEC:-900}"

    nohup python3 -c "
import sys
sys.path.insert(0, 'mastering_engine')
from app import app
app.run(host='127.0.0.1', port=5050, threaded=False)
" > /tmp/python.log 2>&1 &

    # Wait for Python to start
    for i in {1..30}; do
        if curl -s http://127.0.0.1:5050/health > /dev/null 2>&1; then
            echo "Python engine ready (MAX_MASTER_DURATION_SEC=${MAX_MASTER_DURATION_SEC})"
            break
        fi
        sleep 1
    done
fi

# Start Node.js server
exec node src/server.js
