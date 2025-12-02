#!/usr/bin/env bash
set -euo pipefail

# Colors (match run.sh style)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Build and run the inplace merge benchmark, capture per-run sort/linear times
# and Max temporary buffer size, and report peak RSS.

USAGE="Usage: $0 [seed_start] [seed_end] [sort=std|kx]"

seed_start=${1:-0}
seed_end=${2:-1}
sort_algo=${3:-kx}

BENCH_DIR=benchmark
TARGET=inplace_bench

# USS monitor interval (seconds)
USSMON_INTERVAL=${USSMON_INTERVAL:-0.005}
# If the benchmark binary already exists, skip configure/build to save time.
if [ -x "./${BENCH_DIR}/${TARGET}" ]; then
    echo "Found existing ${BENCH_DIR}/${TARGET}, skipping build."
else
    echo "Configuring and building into ${BENCH_DIR}..."
    # Ensure a clean build directory
    rm -rf "$BENCH_DIR"
    mkdir -p "$BENCH_DIR"
    cmake -S . -B "$BENCH_DIR"
    # Build only the target to speed up
    cmake --build "$BENCH_DIR" --target "$TARGET" -j$(nproc)
fi

OUTFILE=$(mktemp /tmp/inplace_bench_out.XXXXXX)
ERRFILE=$(mktemp /tmp/inplace_bench_err.XXXXXX)
USSFILE=$(mktemp /tmp/inplace_bench_uss.XXXXXX)
RSSFILE=$(mktemp /tmp/inplace_bench_rss.XXXXXX)

# Monitor USS and RSS peak while process is running
monitor_mem()
{
    local pid="$1"; local uss_out="$2"; local rss_out="$3"; local interval="$4";
    local peak_uss=0
    local peak_rss=0
    # wait briefly for proc to appear
    local max_wait=20
    local wait_count=0
    while [ ! -f "/proc/$pid/smaps_rollup" ] && (( wait_count < max_wait )); do
        sleep 0.005
        wait_count=$((wait_count + 1))
    done
    if [ ! -f "/proc/$pid/smaps_rollup" ]; then
        echo "0" > "$uss_out"
        echo "0" > "$rss_out"
        return 1
    fi
    while [ -f "/proc/$pid/smaps_rollup" ]; do
        # USS from smaps_rollup
        local current_uss_kb=$(awk 'BEGIN{c=0;d=0} /Private_Clean:/ {c=$2} /Private_Dirty:/ {d=$2} END{print c+d}' "/proc/$pid/smaps_rollup" 2>/dev/null)
        # RSS from status
        local current_rss_kb=$(awk '/VmRSS:/ {print $2}' "/proc/$pid/status" 2>/dev/null || echo 0)
        if [ -n "$current_uss_kb" ] && [ "$current_uss_kb" -gt "$peak_uss" ] 2>/dev/null; then
            peak_uss=$current_uss_kb
        fi
        if [ -n "$current_rss_kb" ] && [ "$current_rss_kb" -gt "$peak_rss" ] 2>/dev/null; then
            peak_rss=$current_rss_kb
        fi
        sleep "$interval"
    done
    echo "$peak_uss" > "$uss_out"
    echo "$peak_rss" > "$rss_out"
}

echo "Running: ./${BENCH_DIR}/${TARGET} $seed_start $seed_end $sort_algo --verbose"
# Start benchmark in background and monitor memory
"./${BENCH_DIR}/${TARGET}" "$seed_start" "$seed_end" "$sort_algo" --verbose >"$OUTFILE" 2>"$ERRFILE" &
cmdpid=$!
monitor_mem "$cmdpid" "$USSFILE" "$RSSFILE" "$USSMON_INTERVAL" &
monpid=$!
# Wait for command
wait "$cmdpid"
rc=$?
# Wait for monitor to finish
wait "$monpid" 2>/dev/null || true

# Read peaks
PEAK_USS_KB=$(cat "$USSFILE" 2>/dev/null || echo 0)
PEAK_RSS_KB=$(cat "$RSSFILE" 2>/dev/null || echo 0)

# Parse OUTFILE for per-run data
# We expect lines like:
# Sorting time: X seconds
# Linear scan time: Y seconds
# Max temporary buffer size used: N

# Collect arrays
mapfile -t SORT_TIMES < <(grep "Sorting time:" "$OUTFILE" | awk '{print $(NF-1)}')
mapfile -t LINEAR_TIMES < <(grep "Linear scan time:" "$OUTFILE" | awk '{print $(NF-1)}')
mapfile -t MAX_TMPS < <(grep "Max temporary buffer size used:" "$OUTFILE" | awk '{print $NF}')

# Compute stats
compute_stats() {
    local -n arr=$1
    if [ ${#arr[@]} -eq 0 ]; then
        echo "0 0 0"
        return
    fi
    local sum=0
    local max=0
    for v in "${arr[@]}"; do
        sum=$(awk "BEGIN {print $sum + $v}")
        cmp=$(awk "BEGIN {print ($v > $max)}")
        if [ "$cmp" -eq 1 ]; then
            max=$v
        fi
    done
    local avg=$(awk "BEGIN {print $sum / ${#arr[@]}}")
    echo "$sum $avg $max"
}

# For integer arrays (MAX_TMPS)
compute_stats_int() {
    local -n arr=$1
    if [ ${#arr[@]} -eq 0 ]; then
        echo "0 0 0"
        return
    fi
    local sum=0
    local max=0
    for v in "${arr[@]}"; do
        sum=$((sum + v))
        if [ "$v" -gt "$max" ]; then
            max=$v
        fi
    done
    local avg=$(awk "BEGIN {printf \"%.4f\", $sum / ${#arr[@]}}")
    echo "$sum $avg $max"
}

read SUM_SORT AVG_SORT MAX_SORT < <(compute_stats SORT_TIMES)
read SUM_LIN AVG_LIN MAX_LIN < <(compute_stats LINEAR_TIMES)
read SUM_TMP AVG_TMP MAX_TMP < <(compute_stats_int MAX_TMPS)

NUM_RUNS=${#SORT_TIMES[@]}

echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}  Inplace Merge Benchmark Summary${NC}"
echo -e "${BLUE}======================================${NC}"
echo
printf "%-24s: %s\n" "Target" "$TARGET"
printf "%-24s: %s - %s\n" "Seed range" "$seed_start" "$((seed_end-1))"
printf "%-24s: %s\n" "Sort" "$sort_algo"
printf "%-24s: %d\n" "Runs" "$NUM_RUNS"
echo
printf "%-24s %12s %12s\n" "Metric" "Average" "Max"
printf "%-24s %12s %12s\n" "------------------------" "------------" "------------"
printf "%-24s %12.4f %12.4f\n" "Sort time (s)" "$AVG_SORT" "$MAX_SORT"
printf "%-24s %12.4f %12.4f\n" "Linear scan (s)" "$AVG_LIN" "$MAX_LIN"
printf "%-24s %12.4f %12d\n" "Temporary buffer (items)" "$AVG_TMP" "$MAX_TMP"
PEAK_USS_MB=$(awk -v v="$PEAK_USS_KB" 'BEGIN {printf "%.4f", v/1024}')
PEAK_RSS_MB=$(awk -v v="$PEAK_RSS_KB" 'BEGIN {printf "%.4f", v/1024}')
printf "%-24s %12s %12s\n" "Peak USS (MB)" "N/A" "$PEAK_USS_MB"
printf "%-24s %12s %12s\n" "Peak RSS (MB)" "N/A" "$PEAK_RSS_MB"
echo
echo -e "${GREEN}Benchmark complete.${NC}"

# cleanup temporary files
rm -f "$OUTFILE" "$ERRFILE" "$USSFILE" "$RSSFILE"

# Keep outputs for inspection; do not delete
exit 0
