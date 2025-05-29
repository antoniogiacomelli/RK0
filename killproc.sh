#!/usr/bin/env bash
# Usage: ./killproc.sh procname

if [ -z "$1" ]; then
  echo "Usage: $0 <process-name>"
  exit 1
fi

PROC="$1"

# Find matching PIDs, ignoring the grep process itself
PIDS=$(ps -A -o pid,comm | grep "[${PROC:0:1}]${PROC:1}" | awk '{print $1}')

if [ -z "$PIDS" ]; then
  echo "No process named '$PROC' found."
  exit 0
fi

echo "Killing process '$PROC' with PID(s): $PIDS"
for PID in $PIDS; do
  kill -9 "$PID" && echo "  → Sent SIGKILL to $PID"
done
