#!/bin/bash

# mem_monitor.sh
# Lightweight script to run a program, monitor its USS (unique set size)
# via /proc/<pid>/smaps_rollup and record the peak. The target program's
# stdout/stderr are redirected to a timestamped logfile so they don't pollute
# this script's stdout output (which is used for monitor messages).
#
# Usage:
#   ./mem_monitor.sh <program> [args...]
# Example:
#   ./mem_monitor.sh ./apr --mode=cip --iters=5 --seed=42

set -u

if [ "$#" -eq 0 ]; then
    echo "Error: no command provided."
    echo "Usage: $0 <command> [args...]"
    exit 1
fi

# Build a logfile path (timestamped) in the current directory
LOG="$(pwd)/mem_monitor_$(date +%Y%m%d_%H%M%S).log"

# Print the exact command we will run (for reproducibility)
echo "Running command (output redirected to $LOG):"
printf '  %q' "$@"
echo " \n--> logs: $LOG"

# Launch the target program with stdout/stderr redirected to the logfile.
# We run it in background so we can poll /proc for memory usage.
"$@" >"$LOG" 2>&1 &
PID=$!

echo "Monitoring process PID: $PID"

# Peak USS observed (KB)
peak_uss_kb=0

# Poll frequency in seconds (can be fractional)
POLL_INTERVAL=0.01

while ps -p $PID > /dev/null; do
    # Prefer smaps_rollup if available (aggregated per-process counts)
    if [ -f "/proc/$PID/smaps_rollup" ]; then
        private_dirty_kb=$(grep "Private_Dirty:" /proc/$PID/smaps_rollup | awk '{print $2}')
        private_clean_kb=$(grep "Private_Clean:" /proc/$PID/smaps_rollup | awk '{print $2}')

        private_dirty_kb=${private_dirty_kb:-0}
        private_clean_kb=${private_clean_kb:-0}

        current_uss_kb=$((private_dirty_kb + private_clean_kb))

        if (( current_uss_kb > peak_uss_kb )); then
            peak_uss_kb=$current_uss_kb
            echo "[$(date +%H:%M:%S)] New peak USS: ${peak_uss_kb} KB"
        fi
    fi
    sleep $POLL_INTERVAL
done

# Wait for the child to finish and capture exit code
wait $PID
exit_code=$?

echo "-------------------------------------"
echo "Process (PID: $PID) exited with code: $exit_code"
echo "Program stdout/stderr were redirected to: $LOG"

if (( peak_uss_kb > 0 )); then
    peak_uss_mb=$(awk "BEGIN {printf \"%.2f\", ${peak_uss_kb}/1024}")
    echo "Peak memory (KB): ${peak_uss_kb} KB (${peak_uss_mb} MB)"
else
    echo "No memory usage data collected. The process may have exited immediately or /proc is not readable."
fi

exit $exit_code