#include "advanced_pr.h"
#include <chrono>
#define IFV if (verbose_logging)

const size_t MAX_LIST_SIZE = 2200000;
const size_t INITIAL_LIST_SIZE = 2097152;
SortAlgo g_sort_algo = SortAlgo::KXSORT;

// optimization parameters
// const size_t BENCHMARK_MOVE_BOUND = 1<<10;
// const size_t BENCHMARK_TMP_SIZE = (1<<10) + 128;
// const size_t BENCHMARK_GROUP_BOUND = 256;

const size_t BENCHMARK_MOVE_BOUND = 1;
const size_t BENCHMARK_TMP_SIZE = 256;
const size_t BENCHMARK_GROUP_BOUND = 256;
bool verbose_logging = false;

// the same as merge_inplace_generic (merge0_inplace)
// `src_arr` and `dst_arr` share the same memory region for memory-efficiency.
template <const bool discard_zero = true, size_t move_bound = BENCHMARK_MOVE_BOUND, const size_t max_tmp_size = BENCHMARK_TMP_SIZE>
inline void merge_inplace_benchmark(LayerVec<Item0> &src_arr, LayerVec<Item1> &dst_arr)
{
    typedef Item0 SrcItem;
    typedef Item1 DstItem;
    if (src_arr.empty())
        return;
    const size_t N = src_arr.size();
    const size_t sz_src = sizeof(SrcItem), sz_dst = sizeof(DstItem);

    std::vector<DstItem> tmp_items;
    tmp_items.reserve(max_tmp_size);
    std::vector<uint8_t> skip_buf;
    skip_buf.reserve(BENCHMARK_GROUP_BOUND);
    size_t free_bytes = 0;
    size_t avail_dst = dst_arr.capacity() - dst_arr.size();

    auto sort_start_time = std::chrono::high_resolution_clock::now();
    sort20(src_arr);
    auto sort_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sort_duration = sort_end_time - sort_start_time;
    IFV
    {
        std::cout << "Sorting time: " << sort_duration.count() << " seconds" << std::endl;
    }
    size_t i = 0;
    size_t real_max_tmp_size = 0;

    auto linear_scan_start_time = std::chrono::high_resolution_clock::now();
    while (i < N)
    {
        const size_t group_start = i;
        const auto key0 = getKey20(src_arr[group_start]);
        i++;
        while (i < N && getKey20(src_arr[i]) == key0)
            ++i;
        const size_t group_end = i;
        const size_t group_size = group_end - group_start;

        if (discard_zero)
        {
            skip_buf.assign(group_size, 0);
            for (size_t j1 = group_start; j1 < group_end; ++j1)
            {
                if (skip_buf[j1 - group_start])
                    continue;
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    if (skip_buf[j2 - group_start])
                        continue;
                    DstItem out = merge_item0(src_arr[j1], src_arr[j2]);
                    if (is_zero_item(out))
                    {
                        skip_buf[j2 - group_start] = 1;
                        continue;
                    }
                    tmp_items.emplace_back(out);
                }
            }
        }
        else
        {
            if (group_size > 3)
            {
                continue;
            } // last hop: skip large groups since they are trivial solutions with overwhelming prob.
            for (size_t j1 = group_start; j1 < group_end; ++j1)
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    tmp_items.emplace_back(merge_item0(src_arr[j1], src_arr[j2]));
                }
        }

        const size_t tmp_size = tmp_items.size();
        if (tmp_size >= avail_dst)
        // already full, we will throw away remaining tmp_items
        {
            break;
        }
        if (tmp_size > real_max_tmp_size)
        {
            real_max_tmp_size = tmp_size;
        }
        free_bytes += group_size * sz_src;
        const size_t can_dst = free_bytes / sz_dst;
        const size_t to_move = std::min<size_t>({tmp_size, can_dst, avail_dst});
        if (to_move >= move_bound)
        // trade-off: to avoid too-frequent small moves, we only move when we have enough items.
        {
            drain_vectors(tmp_items, dst_arr, to_move);
            free_bytes -= to_move * sz_dst;
            avail_dst -= to_move;
        }
    }
    if (tmp_items.size())
    {
        // const size_t avail_dst = dst_arr.capacity() - dst_arr.size();
        if (tmp_items.size() > real_max_tmp_size)
        {
            real_max_tmp_size = tmp_items.size();
        }
        const size_t to_move = std::min(tmp_items.size(), avail_dst);
        drain_vectors(tmp_items, dst_arr, to_move);
    }
    auto linear_scan_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> linear_scan_duration = linear_scan_end_time - linear_scan_start_time;
    IFV
    {
        std::cout << "Linear scan time: " << linear_scan_duration.count() << " seconds" << std::endl;
        std::cout << "Max temporary buffer size used: " << real_max_tmp_size << std::endl;
    }
}

void benchmark_merge_inplace(int seed_start, int seed_end, const std::string &sort_name)
{
    // Setup random number generation
    std::cout << "-------------------------------------------------------------------------------" << std::endl;
    std::cout << "Testing " << sort_name << " inplace merge with MT seed range: " << seed_start << " to " << seed_end - 1 << std::endl;
    size_t total_bytes = MAX_LIST_SIZE * sizeof(Item0);
    uint8_t *base = static_cast<uint8_t *>(std::malloc(total_bytes));
    if (base == nullptr)
    {
        std::cerr << "Failed to allocate " << (total_bytes / (1024 * 1024)) << " MB of memory" << std::endl;
        return;
    }
    // Create the array
    LayerVec<Item0> src_array = init_layer<Item0>(base, total_bytes);
    LayerVec<Item1> dst_array = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
    double total_time = 0.0;
    for (int mt_seed = seed_start; mt_seed < seed_end; ++mt_seed)
    {
        fill_layer_from_mt<LayerVec<Item0>>(src_array, mt_seed);
        auto start_time = std::chrono::high_resolution_clock::now();
        merge_inplace_benchmark(src_array, dst_array);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;
        // std::cout << "Merge finished in " << elapsed.count() << " seconds." << std::endl;
        total_time += elapsed.count();
        src_array.resize(0);
        dst_array.resize(0);
    }
    double avg_time = total_time / (seed_end - seed_start);
    std::cout << "Average merge time over " << (seed_end - seed_start) << " runs: " << avg_time << " seconds." << std::endl;
    std::free(base);
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0] << " <seed_start> <seed_end> <sort_algo> (--verbose)" << std::endl;
        std::cerr << "  <seed_start>: Starting seed for MT random generation" << std::endl;
        std::cerr << "  <seed_end>: Ending seed (exclusive) for MT random generation" << std::endl;
        std::cerr << "  <sort_algo>: Sorting algorithm to use ('std' or 'kx')" << std::endl;
        std::cerr << "  (--verbose): Optional flag to enable verbose logging" << std::endl;
        return 1;
    }
    int mt_seed_start = std::stoi(argv[1]);
    int mt_seed_end = std::stoi(argv[2]);
    std::string sort_algo = argv[3];

    if (argc >= 5)
    {
        std::string verbose_flag = argv[4];
        if (verbose_flag == "--verbose")
        {
            // Enable verbose logging
            // Note: In this simplified example, we use a global constant.
            // In a real implementation, consider using a more flexible logging mechanism.
            verbose_logging = true; // Cannot change const variable, so we rely on the existing setting.
        }
    }

    if (sort_algo == "std")
    {
        g_sort_algo = SortAlgo::STD;
        benchmark_merge_inplace(mt_seed_start, mt_seed_end, "std");
    }
    else if (sort_algo == "kx")
    {
        g_sort_algo = SortAlgo::KXSORT;
        benchmark_merge_inplace(mt_seed_start, mt_seed_end, "kx");
    }
    else
    {
        std::cerr << "Unknown sort algorithm: " << sort_algo << std::endl;
        return 1;
    }
    return 0;
}

// g++ -I./include -I./third_party/kxsort -I./third_party/blake2-avx2 src/inplace_merge_benchmark.cpp third_party/blake2-avx2/blake2bip.c third_party/blake/blake2b.cpp -o benchmark_merge -std=c++17 -O3 -march=native -mavx2 -funroll-loops -Wall -Wextra -Wno-unused-variable