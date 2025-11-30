#!/bin/bash
# Lightweight benchmark for Equihash CIP-APR switching height curve
# Usage: ./run_apr_curve.sh [--nk 144_5|200_9] [--seed S] [--iters N] [--heights "0 1 2 3 4 ..."]
#        --nk      Which Equihash params to use: 144_5 or 200_9 (default 144_5)
#        --seed    Starting seed (default 0)
#        --iters   Number of iterations per height (default 1)
#        --heights Space-separated list of switching heights to test
#                  default: "0 1 2 3 4" for 144_5, "0 1 2 3 4 5 6 7 8" for 200_9
set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; CYAN='\033[36m'; NC='\033[0m'
BOLD='\033[1m'

NK="144_5"          # 144_5 or 200_9
SEED=0
ITERS=1
HEIGHTS=""
HEIGHTS_SET=0       # Tracks whether --heights was explicitly provided

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    --nk)
      NK="$2"
      shift 2
      ;;
    --seed)
      SEED="$2"
      shift 2
      ;;
    --iters)
      ITERS="$2"
      shift 2
      ;;
    --heights)
      HEIGHTS="$2"
      HEIGHTS_SET=1
      shift 2
      ;;
    -h|--help)
      grep '^# Usage:' "$0" | sed 's/^# //' 
      exit 0
      ;;
    *)
      echo -e "${RED}Unknown argument: $1${NC}"
      exit 1
      ;;
  esac
done

# Decode NK -> N, K
case "$NK" in
  144_5)
    NVAL=144
    KVAL=5
    ;;
  200_9)
    NVAL=200
    KVAL=9
    ;;
  *)
    echo -e "${RED}Unsupported --nk value: ${NK} (must be 144_5 or 200_9)${NC}"
    exit 1
    ;;
esac

# Apply default heights per K when --heights is omitted
if [ "$HEIGHTS_SET" -eq 0 ]; then
  if [ "$NK" = "144_5" ]; then
    HEIGHTS="0 1 2 3 4"
  else
    HEIGHTS="0 1 2 3 4 5 6 7 8"
  fi
fi

MAIN_BIN="apr_${NK}"
HMAX=$((KVAL - 1))
SMAPS_WARNED=0
RUN_OUTPUT=""
MAX_USS_KB=0

measure_uss_and_capture_output() {
  local tmp_out
  tmp_out=$(mktemp)
  local max_uss=0

  "$@" >"$tmp_out" 2>&1 &
  local pid=$!

  while kill -0 "$pid" 2>/dev/null; do
    if [ -r "/proc/$pid/smaps_rollup" ]; then
      local uss
      uss=$(cat "/proc/$pid/smaps_rollup" 2>/dev/null | awk '/^Private_/ {uss+=$2} END {print uss+0}')
      [ -z "$uss" ] && uss=0
      if [ "$uss" -gt "$max_uss" ]; then
        max_uss="$uss"
      fi
    else
      if [ "$SMAPS_WARNED" -eq 0 ]; then
        echo -e "${YELLOW}Warning: /proc/*/smaps_rollup not readable; USS measurements will be 0.${NC}"
        SMAPS_WARNED=1
      fi
    fi
    sleep 0.05
  done

  wait "$pid"
  local exit_code=$?
  RUN_OUTPUT=$(cat "$tmp_out")
  MAX_USS_KB="$max_uss"
  rm -f "$tmp_out"
  return $exit_code
}

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
printf "${BLUE}${BOLD}┃%-53s┃${NC}\n" "  Equihash(${NVAL},${KVAL}) CIP-APR Switching Height Curve"
echo -e "${BLUE}${BOLD}┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛${NC}"
echo -e "Params: ${CYAN}${NK}${NC}  Seed: ${CYAN}${SEED}${NC}  Iters: ${CYAN}${ITERS}${NC}  Heights: ${CYAN}${HEIGHTS}${NC}"
echo ""

# Results storage
declare -a RESULTS

echo -e "${GREEN}Running CIP-APR with different switching heights...${NC}"
echo -e "${YELLOW}────────────────────────────────────────────────────────${NC}"

for h in $HEIGHTS; do
  echo -ne "${CYAN}[h=${h}]${NC} Running ${ITERS} iteration(s)... "
  
  # Run while sampling USS (Private_Clean + Private_Dirty)
  measure_uss_and_capture_output ./build/${MAIN_BIN} --mode=cip-apr --h=${h} --seed=${SEED} --iters=${ITERS}
  OUTPUT="$RUN_OUTPUT"
  PEAK_USS="$MAX_USS_KB"
  
  # Parse results
  TOTAL_SOLS=$(echo "$OUTPUT" | grep "total_sols=" | sed -E 's/.*total_sols=([0-9]+).*/\1/')
  TIME_S=$(echo "$OUTPUT" | grep "single_run_time=" | sed -E 's/.*single_run_time=([0-9.]+).*/\1/')
  SOL_PER_SEC=$(echo "$OUTPUT" | grep "Sol/s=" | sed -E 's/.*Sol\/s=([0-9.]+).*/\1/')
  
  [ -z "$PEAK_USS" ] && PEAK_USS=0
  [ -z "$TOTAL_SOLS" ] && TOTAL_SOLS=0
  [ -z "$TIME_S" ] && TIME_S=0
  [ -z "$SOL_PER_SEC" ] && SOL_PER_SEC=0
  
  PEAK_MB=$(awk -v v="$PEAK_USS" 'BEGIN{printf "%.1f", v/1024}')
  
  echo -e "Done. ${GREEN}sols=${TOTAL_SOLS}${NC} time=${TIME_S}s sol/s=${SOL_PER_SEC} USS=${PEAK_MB}MB"
  
  RESULTS+=("${h}|${TIME_S}|${TOTAL_SOLS}|${SOL_PER_SEC}|${PEAK_USS}|${PEAK_MB}")
done

echo ""
echo -e "${GREEN}${BOLD}Results Summary${NC}"
echo -e "${YELLOW}────────────────────────────────────────────────────────${NC}"
printf "${BOLD}%-8s %12s %10s %10s %15s %12s${NC}\n" "Height" "Time(s)" "Solutions" "Sol/s" "PeakUSS(kB)" "PeakUSS(MB)"
printf "%-8s %12s %10s %10s %15s %12s\n" "--------" "------------" "----------" "----------" "---------------" "------------"

for r in "${RESULTS[@]}"; do
  IFS='|' read -r h time sols solps uss mb <<< "$r"
  printf "%-8s %12s %10s %10s %15s %12s\n" "h=${h}" "${time}" "${sols}" "${solps}" "${uss}" "${mb}"
done

echo ""
echo -e "${BLUE}Notes:${NC}"
echo "  - Higher switching_height = lower memory, longer time (more recomputation)"
echo "  - h=0: No partial recomputation (baseline memory)"
echo "  - h=${HMAX}: Maximum recomputation (minimum memory) for Equihash(${NVAL},${KVAL})"
echo "  - Valid h range: 0 .. ${HMAX}"

echo -e "${GREEN}Benchmark complete.${NC}"
