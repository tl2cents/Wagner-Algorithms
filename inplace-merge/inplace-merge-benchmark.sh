#!/bin/bash
set -euo pipefail

### Build settings
# If you want to force rebuild, set REBUILD=1 in environment when running this script.
REBUILD=${REBUILD:-0}

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

# Path to kxsort headers (relative to repo layout)
# The kxsort headers live in $PROJECT_ROOT/cip-poc/third_party/kxsort,
# but `fast_inplace_merge.cpp` includes "./kxsort/kxsort.h" so the include
# path should be the parent directory (third_party) so that "kxsort/kxsort.h"
# resolves correctly.
KXSORT_INCLUDE="$PROJECT_ROOT/cip-poc/third_party"

# Compiler / flags
CPP=g++
CPPFLAGS="-std=c++11 -O3 -march=native -I${KXSORT_INCLUDE}"

EXE=$SCRIPT_DIR/merge-inplace
# Allow overriding N_RUNS from environment for quick tests: e.g. N_RUNS=5
N_RUNS=${N_RUNS:-200}
SORTS=("kx" "std")

echo "Running benchmark for $EXE ..."
echo "Seed range: 1 - $N_RUNS"
echo "---------------------------------------------"

# Auto-build step: compile fast_inplace_merge.cpp if binary missing or REBUILD requested
if [[ ! -x "$EXE" || "$REBUILD" == "1" ]]; then
    echo "Executable $EXE not found or rebuild requested. Compiling..."
    SRC="$SCRIPT_DIR/fast_inplace_merge.cpp"
    if [[ ! -f "$SRC" ]]; then
        echo "Error: source file $SRC not found." >&2
        exit 1
    fi

    echo "Using include path: $KXSORT_INCLUDE"
    echo "Compile command: $CPP $CPPFLAGS -o $EXE $SRC"
    if ! $CPP $CPPFLAGS -o "$EXE" "$SRC"; then
        echo "Compilation failed." >&2
        exit 2
    fi
    echo "Compiled $EXE successfully."
fi

# 保存最终统计结果的数组
declare -A results

# 计算 min/max/avg
calc_stats() {
    local values=("$@")
    local sum=0
    local min=999999999
    local max=0
    local count=${#values[@]}
    for v in "${values[@]}"; do
        [[ -z "$v" ]] && continue
        (( $(echo "$v < $min" | bc -l) )) && min=$v
        (( $(echo "$v > $max" | bc -l) )) && max=$v
        sum=$(echo "$sum + $v" | bc -l)
    done
    local avg
    avg=$(echo "scale=6; $sum / $count" | bc -l)
    printf "%.6f %.6f %.6f" "$min" "$max" "$avg"
}

run_tests() {
    local sort_name=$1
    local sort_time_list=()
    local merge_time_list=()
    local max_group_list=()
    local mem_peak_list=()

    for ((seed=1; seed<=N_RUNS; seed++)); do
        echo "[$sort_name] Running seed=$seed ..."
        # 直接捕获输出，不落盘
        output=$(/usr/bin/time -v "$EXE" "$seed" "$sort_name" 2>&1 || true)

        # 解析
        sort_time=$(grep -oP "${sort_name//:/::?}.*finished in \K[0-9.]+(?= seconds)" <<< "$output" | head -n1)
        merge_time=$(grep -oP "Merging Sorted Array finished in \K[0-9.]+(?= seconds)" <<< "$output" | head -n1)
        max_group=$(grep -oP "Max group size during merge: \K[0-9]+" <<< "$output" | head -n1)
        mem_peak=$(grep -oP "Maximum resident set size.*: \K[0-9]+" <<< "$output" | tail -n1)

        sort_time_list+=("$sort_time")
        merge_time_list+=("$merge_time")
        max_group_list+=("$max_group")
        mem_peak_list+=("$mem_peak")
    done

    echo "Processing stats for $sort_name ..."

    read -r min max avg <<< "$(calc_stats "${sort_time_list[@]}")"
    results["$sort_name-sort_time"]="$min $max $avg"

    read -r min max avg <<< "$(calc_stats "${merge_time_list[@]}")"
    results["$sort_name-merge_time"]="$min $max $avg"

    read -r min max avg <<< "$(calc_stats "${max_group_list[@]}")"
    results["$sort_name-max_group"]="$min $max $avg"

    read -r min max avg <<< "$(calc_stats "${mem_peak_list[@]}")"
    results["$sort_name-mem_peak_kb"]="$min $max $avg"
}

for s in "${SORTS[@]}"; do
    run_tests "$s"
done

# Auto-build step: compile fast_inplace_merge.cpp if binary missing or REBUILD requested
if [[ ! -x "$EXE" || "$REBUILD" == "1" ]]; then
    echo "Executable $EXE not found or rebuild requested. Compiling..."
    SRC="$SCRIPT_DIR/fast_inplace_merge.cpp"
    if [[ ! -f "$SRC" ]]; then
        echo "Error: source file $SRC not found." >&2
        exit 1
    fi

    echo "Using include path: $KXSORT_INCLUDE"
    echo "Compile command: $CPP $CPPFLAGS -o $EXE $SRC"
    if ! $CPP $CPPFLAGS -o "$EXE" "$SRC"; then
        echo "Compilation failed." >&2
        exit 2
    fi
    echo "Compiled $EXE successfully."
fi

echo "---------------------------------------------"
echo "✅ Benchmark completed."

# 打印格式化汇总表格
echo ""
echo "============== Formatted Summary =============="
printf "%-8s | %-12s | %-10s | %-10s | %-10s\n" "Sort" "Metric" "Min" "Max" "Avg"
echo "-------------------------------------------------------------"

for s in "${SORTS[@]}"; do
    for metric in sort_time merge_time max_group mem_peak_kb; do
        read -r min max avg <<< "${results["$s-$metric"]}"
        printf "%-8s | %-12s | %-10s | %-10s | %-10s\n" "$s" "$metric" "$min" "$max" "$avg"
    done
done

echo "-------------------------------------------------------------"
echo "🎯 All results saved"
