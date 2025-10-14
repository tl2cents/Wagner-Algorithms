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
SEED=42

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
if [ ! -f "build/eq" ]; then
    echo -e "${RED}Error: build/eq not found. Please run 'make' first.${NC}"
    exit 1
fi

echo -e "${GREEN}Running benchmarks...${NC}"
echo ""

# Temporary file to store results
TMPFILE=$(mktemp)

# Run our implementations
echo -e "${YELLOW}[1/4] Running CIP...${NC}"
./build/eq --mode=cip --iters=$ITERS --seed=$SEED 2>&1 | tee -a $TMPFILE

echo ""
echo -e "${YELLOW}[2/4] Running CIP-PR...${NC}"
./build/eq --mode=cip-pr --iters=$ITERS --seed=$SEED 2>&1 | tee -a $TMPFILE

echo ""
echo -e "${YELLOW}[3/4] Running CIP-EM...${NC}"
./build/eq --mode=cip-em --iters=$ITERS --seed=$SEED 2>&1 | tee -a $TMPFILE

echo ""

# Run Tromp's baseline if available
TROMP_RESULT=""
if [ -d "equihash" ]; then
    echo -e "${YELLOW}[4/4] Running Tromp's Baseline...${NC}"
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
printf "%-20s %10s %15s\n" "Algorithm" "Sol/s" "PeakRSS (kB)"
printf "%-20s %10s %15s\n" "--------------------" "----------" "---------------"

# Extract data from our implementations
grep "mode=cip " $TMPFILE | awk -F'[ =]' '{
    for(i=1;i<=NF;i++) {
        if($i=="Sol/s") sols=$(i+1);
        if($i=="peakRSS_kB") rss=$(i+1);
    }
    printf "%-20s %10.2f %15d\n", "CIP", sols, rss
}'

grep "mode=cip-pr " $TMPFILE | awk -F'[ =]' '{
    for(i=1;i<=NF;i++) {
        if($i=="Sol/s") sols=$(i+1);
        if($i=="peakRSS_kB") rss=$(i+1);
    }
    printf "%-20s %10.2f %15d\n", "CIP-PR", sols, rss
}'

grep "mode=cip-em " $TMPFILE | awk -F'[ =]' '{
    for(i=1;i<=NF;i++) {
        if($i=="Sol/s") sols=$(i+1);
        if($i=="peakRSS_kB") rss=$(i+1);
    }
    printf "%-20s %10.2f %15d\n", "CIP-EM", sols, rss
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
echo ""

# Cleanup
rm -f $TMPFILE

echo -e "${GREEN}Benchmark complete!${NC}"
