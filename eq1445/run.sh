#!/bin/bash
# Unified benchmark script for Equihash CIP variants + Tromp baseline
# Usage: ./run.sh [-x4] [--iters N] [--seed S] [--variant=200_9|144_5]
#        -x4       Enable 4-way BLAKE2b SIMD optimized build (requires AVX2, 200_9 only)
#        --iters N Number of iterations (default 5)
#        --seed S  Starting seed (default 0)
#        --variant Select parameter set (default 200_9)
# Environment: USSMON_INTERVAL can override default 0.01
set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; MAGENTA='\033[35m'; CYAN='\033[36m'; NC='\033[0m'
BOLD='\033[1m'

ITERS=2
SEED=42
X4_MODE=false
VARIANT="200_9"
USSMON_INTERVAL=${USSMON_INTERVAL:-0.01}

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    -x4) X4_MODE=true; shift ;;
    --iters) ITERS="$2"; shift 2 ;;
    --seed) SEED="$2"; shift 2 ;;
    --variant) VARIANT="$2"; shift 2 ;;
    -h|--help) grep '^# Usage:' "$0" | sed 's/^# //' ; exit 0 ;;
    *) echo -e "${RED}Unknown argument: $1${NC}"; exit 1 ;;
  esac
done

if [ ! -d build ]; then
  echo -e "${YELLOW}Build directory not found. Creating...${NC}";
  mkdir -p build
fi

HAS_CIP_APR=true
VARIANT_LABEL="Equihash(200,9)"
TROMP_TARGET=""
TROMP_BIN=""

case "$VARIANT" in
  200_9)
    VARIANT_LABEL="Equihash(200,9)"
    if $X4_MODE; then
      MAIN_BIN="apr_x41"
      EM_FILE="./ip_cache_x41.bin"
      TROMP_TARGET="equix41"
      TROMP_BIN="equix41"
    else
      MAIN_BIN="apr_200_9"
      EM_FILE="./ip_cache.bin"
      TROMP_TARGET="equi1"
      TROMP_BIN="equi1"
    fi
    ;;
  144_5)
    VARIANT_LABEL="Equihash(144,5)"
    if $X4_MODE; then
      echo -e "${YELLOW}Warning: -x4 is only available for Equihash(200,9); running standard build instead.${NC}"
      X4_MODE=false
    fi
    MAIN_BIN="apr_144_5"
    EM_FILE="./ip_cache_144_5.bin"
    HAS_CIP_APR=false
    TROMP_TARGET="eq14451"
    TROMP_BIN="eq14451"
    ;;
  *)
    echo -e "${RED}Unsupported variant: ${VARIANT}${NC}"
    exit 1
    ;;
esac

# Build project if missing main binary
if [ ! -f "build/${MAIN_BIN}" ]; then
  echo -e "${YELLOW}Main binary build/${MAIN_BIN} missing. Configuring & building...${NC}";
  (cd build && cmake .. && make -j"$(nproc)")
fi

if [ ! -f "build/${MAIN_BIN}" ]; then
  echo -e "${RED}Error: build/${MAIN_BIN} still not found after build.${NC}"; exit 1
fi

# Header formatting
print_header() {
  local mode_note=""
  if $X4_MODE; then
    mode_note="${MAGENTA}Mode: 4-way BLAKE2b SIMD (AVX2 required)${NC}"
    if ! grep -qi avx2 /proc/cpuinfo 2>/dev/null; then
      echo -e "${RED}Warning: AVX2 not detected -> 4-way SIMD may not run optimally.${NC}"
    fi
  else
    mode_note="${CYAN}Mode: Standard BLAKE2b${NC}"
  fi
  echo -e "${BLUE}${BOLD}┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓${NC}"
  printf "${BLUE}${BOLD}┃%-46s┃${NC}\n" "  ${VARIANT_LABEL} Benchmark Suite (CIP)"
  echo -e "${BLUE}${BOLD}┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛${NC}"
  printf "Iterations: %s  Start Seed: %s  Binary: %s\n" "$ITERS" "$SEED" "$MAIN_BIN"
  echo -e "$mode_note"
  echo ""
}

print_header

TMPFILE=$(mktemp)

monitor_uss() {
  local pid="$1" out="$2" interval="$3" peak=0
  local max_wait=40 wait_count=0
  while [ ! -f "/proc/$pid/smaps_rollup" ] && (( wait_count < max_wait )); do
    sleep 0.005; wait_count=$((wait_count+1))
  done
  [ ! -f "/proc/$pid/smaps_rollup" ] && echo "$peak" > "$out" && return 1
  while [ -f "/proc/$pid/smaps_rollup" ]; do
    local current_uss_kb=$(awk 'BEGIN{c=0;d=0} /Private_Clean:/{c=$2} /Private_Dirty:/{d=$2} END{print c+d}' "/proc/$pid/smaps_rollup" 2>/dev/null)
    [ -z "$current_uss_kb" ] && sleep "$interval" && continue
    (( current_uss_kb > peak )) && peak=$current_uss_kb
    sleep "$interval"
  done
  echo "$peak" > "$out"
}

run_and_monitor() {
  local label="$1"; shift; [ "$1" != -- ] && echo "run_and_monitor usage" && return 1; shift
  local ussfile=$(mktemp) outfile=$(mktemp)
  "$@" > "$outfile" 2>&1 &
  local cmdpid=$!
  tail -f "$outfile" >> "$TMPFILE" 2>/dev/null & local tailpid=$!
  monitor_uss "$cmdpid" "$ussfile" "$USSMON_INTERVAL" & local monpid=$!
  wait "$cmdpid"; local rc=$?
  wait "$monpid" 2>/dev/null || true
  kill "$tailpid" 2>/dev/null || true; wait "$tailpid" 2>/dev/null || true
  cat "$outfile"; rm -f "$outfile"
  local peak_uss=0; [ -f "$ussfile" ] && peak_uss=$(cat "$ussfile" 2>/dev/null || echo 0)
  local peak_mb=$(awk -v v="$peak_uss" 'BEGIN{printf "%.2f", v/1024}')
  echo -e "${BLUE}Peak USS: ${peak_uss} kB (${peak_mb} MB)${NC}"
  echo "mode=$label peakUSS_kB=$peak_uss" >> "$TMPFILE"
  rm -f "$ussfile"; return $rc
}

echo -e "${GREEN}Running implementations...${NC}\n"

BAR="${YELLOW}────────────────────────────────────────────${NC}"
step() { echo -e "${YELLOW}[$1]${NC} $2\n$BAR"; }

step "1" "CIP"
run_and_monitor cip -- ./build/${MAIN_BIN} --mode=cip --iters=$ITERS --seed=$SEED

echo ""; step "2" "CIP-PR"
run_and_monitor cip-pr -- ./build/${MAIN_BIN} --mode=cip-pr --iters=$ITERS --seed=$SEED

echo ""; step "3" "CIP-EM"
run_and_monitor cip-em -- ./build/${MAIN_BIN} --mode=cip-em --iters=$ITERS --seed=$SEED --em=${EM_FILE}
rm -f "${EM_FILE}"

if $HAS_CIP_APR; then
  echo ""; step "4" "CIP-APR"
  run_and_monitor cip-apr -- ./build/${MAIN_BIN} --mode=cip-apr --iters=$ITERS --seed=$SEED
fi

echo "";
TROMP_AVAILABLE=false
if [ -n "$TROMP_TARGET" ]; then
  if $HAS_CIP_APR; then
    TROMP_STEP_ID=5
  else
    TROMP_STEP_ID=4
  fi
  if [ -d equihash ]; then
    step "$TROMP_STEP_ID" "Tromp Baseline"
  if [ ! -f "build/${TROMP_BIN}" ]; then
    echo -e "Building Tromp baseline target ${TROMP_TARGET}..."
    (cd equihash && make ${TROMP_TARGET} > /dev/null 2>&1) || echo -e "${RED}Tromp build failed${NC}"
    [ -f "equihash/${TROMP_BIN}" ] && mv "equihash/${TROMP_BIN}" "build/${TROMP_BIN}" || true
  fi
  if [ -f "build/${TROMP_BIN}" ]; then
    TROMP_AVAILABLE=true
    chmod +x "build/${TROMP_BIN}" 2>/dev/null || true
    ussfile=$(mktemp); outfile=$(mktemp)
    START=$(date +%s.%N)
    ./build/${TROMP_BIN} -n $SEED -r $ITERS -s > "$outfile" 2>&1 & cmdpid=$!
    monitor_uss "$cmdpid" "$ussfile" "$USSMON_INTERVAL" & monpid=$!
    wait "$cmdpid"
    END=$(date +%s.%N); TIME=$(echo "$END - $START" | bc)
    wait "$monpid" 2>/dev/null || true
    TOTAL_SOLS=$(grep "total solutions" "$outfile" | awk '{print $1}')
    [ -z "$TOTAL_SOLS" ] && TOTAL_SOLS=0
    if [ "$TIME" != "0" ] && [ -n "$TIME" ]; then SOL_PER_SEC=$(echo "scale=2; $TOTAL_SOLS / $TIME" | bc); else SOL_PER_SEC=0; fi
    if command -v /usr/bin/time >/dev/null 2>&1; then
      TIME_OUTPUT=$(/usr/bin/time -v ./build/${TROMP_BIN} -n $SEED -r $ITERS -s 2>&1 >/dev/null | grep "Maximum resident" || echo "Maximum resident set size (kbytes): 0")
      PEAK_RSS=$(echo "$TIME_OUTPUT" | awk '{print $6}')
    else
      PEAK_RSS=0
    fi
    echo "$TOTAL_SOLS total solutions"; echo "Time: ${TIME}s, Sol/s: ${SOL_PER_SEC}"; rm -f "$outfile"
    peak_uss=0; [ -f "$ussfile" ] && peak_uss=$(cat "$ussfile" 2>/dev/null || echo 0)
    peak_mb=$(awk -v v="$peak_uss" 'BEGIN{printf "%.2f", v/1024}')
    echo -e "${BLUE}Peak USS: ${peak_uss} kB (${peak_mb} MB)${NC}"
    echo "mode=tromp-baseline total_sols=$TOTAL_SOLS time=$TIME Sol/s=$SOL_PER_SEC peakRSS_kB=$PEAK_RSS" >> "$TMPFILE"
    echo "mode=tromp-baseline peakUSS_kB=$peak_uss" >> "$TMPFILE"
    rm -f "$ussfile"
    else
      echo -e "${YELLOW}Tromp baseline binary not found in build/. Skipping.${NC}"; echo "To enable: place or build into build/${TROMP_BIN}"
    fi
  else
    echo -e "${YELLOW}equihash/ source directory not present; Tromp baseline skipped for variant ${VARIANT}.${NC}"
  fi
fi

echo ""; echo -e "${GREEN}${BOLD}Results Summary${NC}"
printf "%-20s %10s %15s %15s\n" "Algorithm" "Sol/s" "PeakRSS(kB)" "PeakUSS(kB)"
printf "%-20s %10s %15s %15s\n" "--------------------" "----------" "---------------" "---------------"

parse_mode() {
  local m="$1"; local name="$2";
  grep "mode=${m} " "$TMPFILE" | awk -v label="$name" -F'[ =]' 'BEGIN{sols=0;rss=0} /mode='"$m"'/ && /Sol\/s/ {for(i=1;i<=NF;i++){if($i=="Sol/s") sols=$(i+1); if($i=="peakRSS_kB") rss=$(i+1)}} END{printf "%-20s %10.2f %15d", label, sols, rss}'
  local uss=$(grep "mode=${m} peakUSS_kB=" "$TMPFILE" | sed -E 's/.*peakUSS_kB=([0-9]+).*/\1/' | tail -1)
  [ -z "$uss" ] && uss=0; printf " %15d\n" "$uss"
}

parse_mode cip "CIP"
parse_mode cip-pr "CIP-PR"
parse_mode cip-em "CIP-EM"
if $HAS_CIP_APR; then parse_mode cip-apr "CIP-APR"; fi
if $TROMP_AVAILABLE; then parse_mode tromp-baseline "Tromp-Baseline"; fi

echo ""; echo -e "${BLUE}Notes:${NC}"; echo "  - 4-way mode employs 4-Way BLAKE2b (AVX2 Required)."; echo "  - USS: Unique Set Size (private memory)."; echo "  - RSS: Resident Set Size (total)."

rm -f "$TMPFILE"

echo -e "${GREEN}Benchmark complete.${NC}"