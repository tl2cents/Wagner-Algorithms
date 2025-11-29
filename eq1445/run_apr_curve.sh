#!/bin/bash
# Lightweight benchmark for Equihash(144,5) CIP-APR switching height curve
# Usage: ./run_apr_curve.sh [--seed S] [--iters N] [--heights "0 1 2 3 4"]
#        --seed S    Starting seed (default 0)
#        --iters N   Number of iterations per height (default 1)
#        --heights   Space-separated list of switching heights to test (default "0 1 2 3 4")
set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; CYAN='\033[36m'; NC='\033[0m'
BOLD='\033[1m'

SEED=0
ITERS=1
HEIGHTS="0 1 2 3 4"
MAIN_BIN="apr_144_5"

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    --seed) SEED="$2"; shift 2 ;;
    --iters) ITERS="$2"; shift 2 ;;
    --heights) HEIGHTS="$2"; shift 2 ;;
    -h|--help) grep '^# Usage:' "$0" | sed 's/^# //' ; exit 0 ;;
    *) echo -e "${RED}Unknown argument: $1${NC}"; exit 1 ;;
  esac
done

# Ensure build directory and binary exist
if [ ! -d build ]; then
  echo -e "${YELLOW}Build directory not found. Creating...${NC}"
  mkdir -p build
fi

if [ ! -f "build/${MAIN_BIN}" ]; then
  echo -e "${YELLOW}Binary build/${MAIN_BIN} missing. Building...${NC}"
  (cd build && cmake .. && make -j"$(nproc)" ${MAIN_BIN})
fi

if [ ! -f "build/${MAIN_BIN}" ]; then
  echo -e "${RED}Error: build/${MAIN_BIN} not found after build.${NC}"
  exit 1
fi

# Print header
echo -e "${BLUE}${BOLD}┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓${NC}"
printf "${BLUE}${BOLD}┃%-53s┃${NC}\n" "  Equihash(144,5) CIP-APR Switching Height Curve"
echo -e "${BLUE}${BOLD}┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛${NC}"
echo -e "Seed: ${CYAN}${SEED}${NC}  Iters: ${CYAN}${ITERS}${NC}  Heights: ${CYAN}${HEIGHTS}${NC}"
echo ""

# Results storage
declare -a RESULTS

echo -e "${GREEN}Running CIP-APR with different switching heights...${NC}"
echo -e "${YELLOW}────────────────────────────────────────────────────────${NC}"

for h in $HEIGHTS; do
  echo -ne "${CYAN}[h=${h}]${NC} Running ${ITERS} iteration(s)... "
  
  # Run with /usr/bin/time to get peak RSS
  OUTPUT=$(/usr/bin/time -v ./build/${MAIN_BIN} --mode=cip-apr --h=${h} --seed=${SEED} --iters=${ITERS} 2>&1)
  
  # Parse results
  PEAK_RSS=$(echo "$OUTPUT" | grep "Maximum resident set size" | awk '{print $6}')
  TOTAL_SOLS=$(echo "$OUTPUT" | grep "total_sols=" | sed -E 's/.*total_sols=([0-9]+).*/\1/')
  TIME_S=$(echo "$OUTPUT" | grep "single_run_time=" | sed -E 's/.*single_run_time=([0-9.]+).*/\1/')
  SOL_PER_SEC=$(echo "$OUTPUT" | grep "Sol/s=" | sed -E 's/.*Sol\/s=([0-9.]+).*/\1/')
  
  [ -z "$PEAK_RSS" ] && PEAK_RSS=0
  [ -z "$TOTAL_SOLS" ] && TOTAL_SOLS=0
  [ -z "$TIME_S" ] && TIME_S=0
  [ -z "$SOL_PER_SEC" ] && SOL_PER_SEC=0
  
  PEAK_MB=$(awk -v v="$PEAK_RSS" 'BEGIN{printf "%.1f", v/1024}')
  
  echo -e "Done. ${GREEN}sols=${TOTAL_SOLS}${NC} time=${TIME_S}s sol/s=${SOL_PER_SEC} mem=${PEAK_MB}MB"
  
  RESULTS+=("${h}|${TIME_S}|${TOTAL_SOLS}|${SOL_PER_SEC}|${PEAK_RSS}|${PEAK_MB}")
done

echo ""
echo -e "${GREEN}${BOLD}Results Summary${NC}"
echo -e "${YELLOW}────────────────────────────────────────────────────────${NC}"
printf "${BOLD}%-8s %12s %10s %10s %15s %12s${NC}\n" "Height" "Time(s)" "Solutions" "Sol/s" "PeakRSS(kB)" "PeakRSS(MB)"
printf "%-8s %12s %10s %10s %15s %12s\n" "--------" "------------" "----------" "----------" "---------------" "------------"

for r in "${RESULTS[@]}"; do
  IFS='|' read -r h time sols solps rss mb <<< "$r"
  printf "%-8s %12s %10s %10s %15s %12s\n" "h=${h}" "${time}" "${sols}" "${solps}" "${rss}" "${mb}"
done

echo ""
echo -e "${BLUE}Notes:${NC}"
echo "  - Higher switching_height = lower memory, longer time (more recomputation)"
echo "  - h=0: No partial recomputation (baseline memory)"
echo "  - h=4: Maximum recomputation (minimum memory)"

echo -e "${GREEN}Benchmark complete.${NC}"
