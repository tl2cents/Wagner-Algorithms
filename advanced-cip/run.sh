#!/bin/bash

# Benchmark script for Equihash(200,9) solver implementations
# Compares CIP, CIP-PR, CIP-EM, and Tromp's baseline

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
ITERS=1000
SEED=0
# USS monitor interval in seconds (can be fractional). Can be overridden by
# environment variable USSMON_INTERVAL when invoking the script.
USSMON_INTERVAL=${USSMON_INTERVAL:-0.01}

echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}  Equihash(200,9) Benchmark Suite${NC}"
echo -e "${BLUE}======================================${NC}"
echo ""
echo "Configuration:"
echo "  - Iterations: $ITERS"
echo "  - Starting seed: $SEED"
echo ""

# Check if build directory exists
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Build directory not found. Creating and building...${NC}"
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc)
    cd ..
    echo ""
fi

# Check if executable exists
if [ ! -f "build/apr" ]; then
    echo -e "${RED}Error: build/apr not found. Please run 'make' first.${NC}"
    exit 1
fi

echo -e "${GREEN}Running benchmarks...${NC}"
echo ""

# Temporary file to store results
TMPFILE=$(mktemp)

# Helper: measure USS for a given PID and write peak (KB) to a file.
# Usage: monitor_uss <pid> <out_file> <interval>
monitor_uss()
{
    local pid="$1"; local out="$2"; local interval="$3";
    local peak=0
    
    # Wait briefly for /proc entry to appear
    local max_wait=20
    local wait_count=0
    while [ ! -f "/proc/$pid/smaps_rollup" ] && (( wait_count < max_wait )); do
        sleep 0.005
        wait_count=$((wait_count + 1))
    done
    
    # Check if we found the process
    if [ ! -f "/proc/$pid/smaps_rollup" ]; then
        echo "$peak" > "$out"
        return 1
    fi
    
    # Monitor while process is alive
    while [ -f "/proc/$pid/smaps_rollup" ]; do
        # Use awk for more efficient parsing
        local current_uss_kb=$(awk '
            BEGIN { clean=0; dirty=0 }
            /Private_Clean:/ {clean=$2}
            /Private_Dirty:/ {dirty=$2}
            END {print clean+dirty}
        ' "/proc/$pid/smaps_rollup" 2>/dev/null)
        
        # If awk fails or returns empty, skip this iteration
        if [ -z "$current_uss_kb" ] || [ "$current_uss_kb" = "0" ]; then
            sleep "$interval"
            continue
        fi
        
        # Update peak if current is higher
        if (( current_uss_kb > peak )); then
            peak=$current_uss_kb
        fi
        sleep "$interval"
    done
    echo "$peak" > "$out"
}

# Helper: run a command, monitor its USS and append a USS line to TMPFILE.
# Usage: run_and_monitor <label> -- <cmd...>
run_and_monitor()
{
    local label="$1";
    shift
    if [ "$1" != -- ]; then
        echo "run_and_monitor usage: run_and_monitor <label> -- <cmd...>"
        return 1
    fi
    shift
    local ussfile=$(mktemp)
    local outfile=$(mktemp)

    # Start the command in background and capture its real PID
    # Redirect to file first, then tail it in background to tee
    "$@" > "$outfile" 2>&1 &
    local cmdpid=$!
    
    # Tail the output file to TMPFILE in background
    tail -f "$outfile" 2>/dev/null >> "$TMPFILE" &
    local tailpid=$!

    # Start USS monitor in background immediately
    monitor_uss "$cmdpid" "$ussfile" "$USSMON_INTERVAL" &
    local monpid=$!

    # Wait for command to finish
    wait "$cmdpid"
    local rc=$?

    # Wait for monitor to finish (it should exit after PID dies)
    wait "$monpid" 2>/dev/null || true
    
    # Kill the tail process and clean up output file
    kill "$tailpid" 2>/dev/null || true
    wait "$tailpid" 2>/dev/null || true
    
    # Display the output to terminal
    cat "$outfile"
    rm -f "$outfile"

    # Read peak USS and append to TMPFILE for downstream parsing
    if [ -f "$ussfile" ]; then
        peak_uss=$(cat "$ussfile" 2>/dev/null || echo 0)
    else
        peak_uss=0
    fi
    
    # Print USS immediately after the process finishes
    echo -e "${BLUE}Peak USS: ${peak_uss} kB ($(awk "BEGIN {printf \"%.2f\", $peak_uss/1024}") MB)${NC}"
    
    echo "mode=$label peakUSS_kB=$peak_uss" >> "$TMPFILE"
    rm -f "$ussfile"
    return $rc
}

# Run our implementations
echo -e "${YELLOW}[1/6] Running CIP...${NC}"
run_and_monitor cip -- ./build/apr --mode=cip --iters=$ITERS --seed=$SEED

echo ""
echo -e "${YELLOW}[2/6] Running CIP-PR...${NC}"
run_and_monitor cip-pr -- ./build/apr --mode=cip-pr --iters=$ITERS --seed=$SEED

echo ""
echo -e "${YELLOW}[3/6] Running CIP-EM...${NC}"
run_and_monitor cip-em -- ./build/apr --mode=cip-em --iters=$ITERS --seed=$SEED

echo ""
echo -e "${YELLOW}[4/6] Running CIP-APR...${NC}"
run_and_monitor cip-apr -- ./build/apr --mode=cip-apr --iters=$ITERS --seed=$SEED

echo ""
echo -e "${YELLOW}[5/6] Running CIP-EMC...${NC}"
run_and_monitor cip-emc -- ./build/apr --mode=cip-emc --iters=$ITERS --seed=$SEED

echo ""

# Run Tromp's baseline if available
TROMP_RESULT=""
if [ -d "equihash" ]; then
    echo -e "${YELLOW}[6/6] Running Tromp's Baseline...${NC}"
    cd equihash

    # Build if needed
    if [ ! -f "equix41" ]; then
        echo "Building Tromp's implementation..."
        make equix41 > /dev/null 2>&1 || {
            echo -e "${RED}Failed to build Tromp's baseline. Skipping...${NC}"
            cd ..
            TROMP_RESULT=""
        }
    fi

    # Only run if build succeeded
    if [ -f "equix41" ]; then
        # Run and measure
        START=$(date +%s.%N)
        RESULT=$(./equix41 -n $SEED -r $ITERS -s 2>&1)
        END=$(date +%s.%N)

        # Extract solutions and calculate metrics
        TOTAL_SOLS=$(echo "$RESULT" | grep "total solutions" | awk '{print $1}')
        TIME=$(echo "$END - $START" | bc)
        SOL_PER_SEC=$(echo "scale=2; $TOTAL_SOLS / $TIME" | bc)
        # print TOTAL_SOLS and avg runtime
        echo "Tromp's Baseline: Total Solutions = $TOTAL_SOLS, Avg Time of Single-Run = $(echo "scale=4; $TIME / $ITERS" | bc)s"

        # Measure peak RSS (run again with time command)
        TIME_OUTPUT=$(command time -v ./equix41 -n $SEED -r $ITERS -s 2>&1 | grep "Maximum resident" || echo "Maximum resident set size (kbytes): 0")
        PEAK_RSS=$(echo "$TIME_OUTPUT" | awk '{print $6}')

        TROMP_RESULT="Tromp-Baseline|$SOL_PER_SEC|$PEAK_RSS"

        cd ..
        echo "$RESULT" | head -5
    else
        cd ..
    fi
else
    echo -e "${YELLOW}[4/4] Tromp's baseline directory not found, skipping...${NC}"
    echo "To enable: Copy Tromp's code to ./equihash/"
fi

echo ""
echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}  Benchmark Results${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""

# Parse results and create table
printf "%-20s %10s %15s %15s\n" "Algorithm" "Sol/s" "PeakRSS (kB)" "PeakUSS (kB)"
printf "%-20s %10s %15s %15s\n" "--------------------" "----------" "---------------" "---------------"

# Extract data from our implementations
grep "mode=cip " $TMPFILE | awk -F'[ =]' '{
    for(i=1;i<=NF;i++) {
        if($i=="Sol/s") sols=$(i+1);
        if($i=="peakRSS_kB") rss=$(i+1);
    }
    # Read USS from the separate line
    uss=0;
}
END {
    getline < "'$TMPFILE'";
    while ((getline line < "'$TMPFILE'") > 0) {
        if (line ~ /mode=cip peakUSS_kB=/) {
            match(line, /peakUSS_kB=([0-9]+)/, arr);
            if (arr[1] != "") uss = arr[1];
            break;
        }
    }
    printf "%-20s %10.2f %15d %15d\n", "CIP", sols, rss, uss
}'

grep "mode=cip-pr " $TMPFILE | awk -F'[ =]' '
BEGIN {sols=0; rss=0; uss=0}
/mode=cip-pr / && /Sol\/s/ {
    for(i=1;i<=NF;i++) {
        if($i=="Sol/s") sols=$(i+1);
        if($i=="peakRSS_kB") rss=$(i+1);
    }
}
/mode=cip-pr peakUSS_kB=/ {
    match($0, /peakUSS_kB=([0-9]+)/, arr);
    if (arr[1] != "") uss = arr[1];
}
END {
    printf "%-20s %10.2f %15d %15d\n", "CIP-PR", sols, rss, uss
}'

grep "mode=cip-em " $TMPFILE | awk -F'[ =]' '
BEGIN {sols=0; rss=0; uss=0}
/mode=cip-em / && /Sol\/s/ {
    for(i=1;i<=NF;i++) {
        if($i=="Sol/s") sols=$(i+1);
        if($i=="peakRSS_kB") rss=$(i+1);
    }
}
/mode=cip-em peakUSS_kB=/ {
    match($0, /peakUSS_kB=([0-9]+)/, arr);
    if (arr[1] != "") uss = arr[1];
}
END {
    printf "%-20s %10.2f %15d %15d\n", "CIP-EM", sols, rss, uss
}'

grep "mode=cip-apr " $TMPFILE | awk -F'[ =]' '
BEGIN {sols=0; rss=0; uss=0}
/mode=cip-apr / && /Sol\/s/ {
    for(i=1;i<=NF;i++) {
        if($i=="Sol/s") sols=$(i+1);
        if($i=="peakRSS_kB") rss=$(i+1);
    }
}
/mode=cip-apr peakUSS_kB=/ {
    match($0, /peakUSS_kB=([0-9]+)/, arr);
    if (arr[1] != "") uss = arr[1];
}
END {
    printf "%-20s %10.2f %15d %15d\n", "CIP-APR", sols, rss, uss
}'

grep "mode=cip-emc " $TMPFILE | awk -F'[ =]' '
BEGIN {sols=0; rss=0; uss=0}
/mode=cip-emc / && /Sol\/s/ {
    for(i=1;i<=NF;i++) {
        if($i=="Sol/s") sols=$(i+1);
        if($i=="peakRSS_kB") rss=$(i+1);
    }
}
/mode=cip-emc peakUSS_kB=/ {
    match($0, /peakUSS_kB=([0-9]+)/, arr);
    if (arr[1] != "") uss = arr[1];
}
END {
    printf "%-20s %10.2f %15d %15d\n", "CIP-EMC", sols, rss, uss
}'

# Add Tromp's result if available
if [ ! -z "$TROMP_RESULT" ]; then
    echo "$TROMP_RESULT" | awk -F'|' '{
        printf "%-20s %10.2f %15d\n", $1, $2, $3
    }'
fi

echo ""
echo -e "${BLUE}Notes:${NC}"
echo "  - Sol/s: Solutions per second (higher is better)"
echo "  - PeakRSS: Peak Resident Set Size in kilobytes (lower is better)"
echo "  - PeakUSS: Peak Unique Set Size in kilobytes (lower is better, more accurate than RSS)"
echo ""

# Cleanup
rm -f $TMPFILE

echo -e "${GREEN}Benchmark complete!${NC}"
