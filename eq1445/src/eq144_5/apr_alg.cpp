#include "eq144_5/equihash_144_5.h"
#include "eq144_5/merge_144_5.h"
#include "eq144_5/sort_144_5.h"
#include "eq144_5/util_144_5.h"

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

// verbose-guarded logging helper
#define IFV if (g_verbose)

uint64_t MAX_IP_MEM_BYTES = static_cast<uint64_t>(MAX_LIST_SIZE) * sizeof(Item_IP);
uint64_t MAX_ITEM_MEM_BYTES = static_cast<uint64_t>(MAX_LIST_SIZE) * sizeof(Item0_IDX);

SortAlgo g_sort_algo = SortAlgo::KXSORT;
bool g_verbose = true;

const size_t ItemSizes[5] = {
    EquihashParams::kLayer0XorBytes,
    EquihashParams::kLayer1XorBytes,
    EquihashParams::kLayer2XorBytes,
    EquihashParams::kLayer3XorBytes,
    EquihashParams::kLayer4XorBytes};

const size_t ItemIDXSizes[5] = {
    EquihashParams::kLayer0XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer1XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer2XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer3XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer4XorBytes + EquihashParams::kIndexBytes};

inline Layer_IP recover_IP(int h, int seed, uint8_t *base)
{
    assert(h >= 1 && h <= 5 && "recover_IP only supports layers 1-5 for (144,5)");

    Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
    Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
    Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
    Layer3 L3 = init_layer<Item3>(base, MAX_LIST_SIZE * sizeof(Item3));
    Layer4 L4 = init_layer<Item4>(base, MAX_LIST_SIZE * sizeof(Item4));
    Layer_IP out_IP = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

    if (h == 1)
    {
        Layer0_IDX L0_IDX = init_layer<Item0_IDX>(base, MAX_LIST_SIZE * sizeof(Item0_IDX));
        fill_layer0(L0_IDX, seed);
        set_index_batch(L0_IDX);
        merge0_inplace_for_ip(L0_IDX, out_IP);
        clear_vec(L0_IDX);
        return out_IP;
    }

    fill_layer0(L0, seed);
    merge0_inplace(L0, L1);
    clear_vec(L0);

    if (h == 2)
    {
        Layer1_IDX L1_IDX = expand_layer_to_idx_inplace<Item1, Item1_IDX>(L1);
        merge1_inplace_for_ip(L1_IDX, out_IP);
        clear_vec(L1_IDX);
        return out_IP;
    }

    merge1_inplace(L1, L2);
    clear_vec(L1);
    if (h == 3)
    {
        Layer2_IDX L2_IDX = expand_layer_to_idx_inplace<Item2, Item2_IDX>(L2);
        merge2_inplace_for_ip(L2_IDX, out_IP);
        clear_vec(L2_IDX);
        return out_IP;
    }

    merge2_inplace(L2, L3);
    clear_vec(L2);
    if (h == 4)
    {
        Layer3_IDX L3_IDX = expand_layer_to_idx_inplace<Item3, Item3_IDX>(L3);
        merge3_inplace_for_ip(L3_IDX, out_IP);
        clear_vec(L3_IDX);
        return out_IP;
    }

    merge3_inplace(L3, L4);
    clear_vec(L3);
    Layer4_IDX L4_IDX = expand_layer_to_idx_inplace<Item4, Item4_IDX>(L4);
    merge4_inplace_for_ip(L4_IDX, out_IP);
    clear_vec(L4_IDX);
    return out_IP;
}

std::vector<Solution> plain_cip(int seed, uint8_t *base)
{
    const size_t total_mem = MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES * 4; // K-1 = 4
    bool own_base = false;
    if (!base)
    {
        base = static_cast<uint8_t *>(std::malloc(total_mem));
        own_base = true;
    }
    IFV
    {
        std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
    }

    Layer0_IDX L0 = init_layer<Item0_IDX>(base, MAX_LIST_SIZE * sizeof(Item0_IDX));
    Layer1_IDX L1 = init_layer<Item1_IDX>(base, MAX_LIST_SIZE * sizeof(Item1_IDX));
    Layer2_IDX L2 = init_layer<Item2_IDX>(base, MAX_LIST_SIZE * sizeof(Item2_IDX));
    Layer3_IDX L3 = init_layer<Item3_IDX>(base, MAX_LIST_SIZE * sizeof(Item3_IDX));
    Layer4_IDX L4 = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));

    Layer_IP IP5 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);
    Layer_IP IP4 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 0 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP3 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP2 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP1 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);

    std::vector<Solution> solutions;

    fill_layer0(L0, seed);
    set_index_batch(L0);
    merge0_ip_inplace(L0, L1, IP1);
    set_index_batch(L1);
    clear_vec(L0);

    merge1_ip_inplace(L1, L2, IP2);
    set_index_batch(L2);
    clear_vec(L1);

    merge2_ip_inplace(L2, L3, IP3);
    set_index_batch(L3);
    clear_vec(L2);

    merge3_ip_inplace(L3, L4, IP4);
    set_index_batch(L4);
    clear_vec(L3);

    merge4_inplace_for_ip(L4, IP5);
    clear_vec(L4);

    IFV
    {
        std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
        std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
        std::cout << "Layer 3 IP size: " << IP3.size() << std::endl;
        std::cout << "Layer 2 IP size: " << IP2.size() << std::endl;
        std::cout << "Layer 1 IP size: " << IP1.size() << std::endl;
    }

    if (!IP5.empty())
    {
        expand_solutions(solutions, IP5);
        expand_solutions(solutions, IP4);
        expand_solutions(solutions, IP3);
        expand_solutions(solutions, IP2);
        expand_solutions(solutions, IP1);
        filter_trivial_solutions(solutions);
    }

    if (own_base)
    {
        std::free(base);
    }
    return solutions;
}

std::vector<Solution> plain_cip_pr(int seed, uint8_t *base)
{
    const size_t total_mem = MAX_ITEM_MEM_BYTES;
    bool own_base = false;
    if (!base)
    {
        base = static_cast<uint8_t *>(std::malloc(total_mem));
        own_base = true;
    }
    IFV
    {
        std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
    }

    std::vector<Solution> solutions;
    Layer_IP IP5 = recover_IP(5, seed, base);
    IFV { std::cout << "Layer 5 IP size: " << IP5.size() << std::endl; }

    if (!IP5.empty())
    {
        expand_solutions(solutions, IP5);
        for (int h = 4; h >= 1; --h)
        {
            Layer_IP IPh = recover_IP(h, seed, base);
            IFV { std::cout << "Layer " << h << " IP size: " << IPh.size() << std::endl; }
            expand_solutions(solutions, IPh);
        }
        filter_trivial_solutions(solutions);
    }

    if (own_base)
    {
        std::free(base);
    }
    return solutions;
}

std::vector<Solution> cip_em(int seed, const std::string &em_path, uint8_t *base)
{
    const size_t total_mem = MAX_ITEM_MEM_BYTES;
    bool own_base = false;
    if (!base)
    {
        base = static_cast<uint8_t *>(std::malloc(total_mem));
        own_base = true;
    }
    IFV
    {
        std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
    }

    Layer0_IDX L0 = init_layer<Item0_IDX>(base, MAX_LIST_SIZE * sizeof(Item0_IDX));
    Layer1_IDX L1 = init_layer<Item1_IDX>(base, MAX_LIST_SIZE * sizeof(Item1_IDX));
    Layer2_IDX L2 = init_layer<Item2_IDX>(base, MAX_LIST_SIZE * sizeof(Item2_IDX));
    Layer3_IDX L3 = init_layer<Item3_IDX>(base, MAX_LIST_SIZE * sizeof(Item3_IDX));
    Layer4_IDX L4 = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
    Layer_IP IP5 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

    IPDiskManifest manifest{};
    EquihashIPDiskWriter writer;
    if (!writer.open(em_path.c_str()))
    {
        std::cerr << "Failed to open EM file: " << em_path << std::endl;
        if (own_base)
        {
            std::free(base);
        }
        return {};
    }

    manifest.ip[0].offset = 0;
    fill_layer0(L0, seed);
    set_index_batch(L0);
    merge0_em_ip_inplace(L0, L1, writer);
    manifest.ip[0].count = L1.size();
    manifest.ip[1].offset = writer.get_current_offset();
    set_index_batch(L1);
    clear_vec(L0);
    IFV { std::cout << "Layer 1 size: " << L1.size() << std::endl; }

    merge1_em_ip_inplace(L1, L2, writer);
    manifest.ip[1].count = L2.size();
    manifest.ip[2].offset = writer.get_current_offset();
    set_index_batch(L2);
    clear_vec(L1);
    IFV { std::cout << "Layer 2 size: " << L2.size() << std::endl; }

    merge2_em_ip_inplace(L2, L3, writer);
    manifest.ip[2].count = L3.size();
    manifest.ip[3].offset = writer.get_current_offset();
    set_index_batch(L3);
    clear_vec(L2);
    IFV { std::cout << "Layer 3 size: " << L3.size() << std::endl; }

    merge3_em_ip_inplace(L3, L4, writer);
    manifest.ip[3].count = L4.size();
    set_index_batch(L4);
    clear_vec(L3);
    IFV { std::cout << "Layer 4 size: " << L4.size() << std::endl; }

    merge4_inplace_for_ip(L4, IP5);
    clear_vec(L4);

    writer.close();

    std::vector<Solution> solutions;
    if (!IP5.empty())
    {
        EquihashIPDiskReader reader;
        if (!reader.open(em_path.c_str()))
        {
            std::cerr << "Cannot open EM file for reading\n";
            if (own_base)
            {
                std::free(base);
            }
            return solutions;
        }

        expand_solutions(solutions, IP5);
        for (int i = 3; i >= 0; --i)
        {
            expand_solutions_from_file(solutions, reader, manifest.ip[i]);
        }
        filter_trivial_solutions(solutions);
        reader.close();
    }

    if (own_base)
    {
        std::free(base);
    }
    return solutions;
}

/**
 * @brief Calculate peak memory for advanced CIP-PR at given switching height
 *
 * For Equihash (144,5) with K=5, ItemIDXSizes = {22, 19, 16, 13, 10} bytes:
 * - switching_height = 0: plain_cip, stores all IP1-IP4 -> MAX_ITEM_MEM_BYTES + 4*MAX_IP_MEM_BYTES
 * - switching_height = 1: stores IP2-IP4, recovers IP1 -> max(Layer3_IDX_size + 3*MAX_IP_MEM, MAX_ITEM_MEM)
 * - switching_height = 2: stores IP3-IP4, recovers IP1-IP2 -> max(Layer3_IDX_size + 2*MAX_IP_MEM, MAX_ITEM_MEM)
 * - switching_height = 3: stores IP4, recovers IP1-IP3 -> max(Layer3_IDX_size + 1*MAX_IP_MEM, MAX_ITEM_MEM)
 * - switching_height = 4: plain_cip_pr, recovers all -> MAX_ITEM_MEM_BYTES
 *
 * Memory optimization strategy:
 * - Forward pass Part 1 uses non-indexed Item types (smaller than indexed types)
 * - Forward pass Part 2 uses indexed Layer types, which shrink as layers increase
 * - Peak indexed layer is Layer3_IDX (13+4=17 bytes), not Layer0_IDX (22+4=26 bytes)
 * - Layer buffer: MAX_LIST_SIZE * ItemIDXSizes[K-2] = MAX_LIST_SIZE * 13 bytes
 * - IP storage: (K-1-h) arrays, each MAX_IP_MEM_BYTES, allocated from buffer end backwards
 * - Lower bound: MAX_ITEM_MEM_BYTES to handle recover_IP() which may use Layer0_IDX
 *
 * Memory layout: [Layer buffer (reused)]...[IP{K-1}][IP{K-2}]...[IP{h+1}]
 *                ^base                                                   ^base_end
 *                       First generated IP{h+1} at rightmost position ──┘
 */
uint64_t advanced_cip_pr_peak_memory(int h)
{
    uint64_t k = EquihashParams::K;
    if (h >= static_cast<int>(k) - 1)
    {
        // plain_cip_pr: recovers all IP layers on-demand, no storage needed
        return MAX_ITEM_MEM_BYTES;
    }
    uint64_t ip_storage = MAX_IP_MEM_BYTES * (k - 1 - h);
    uint64_t total_mem = MAX_LIST_SIZE * ItemIDXSizes[k - 2] + ip_storage;
    return total_mem > MAX_ITEM_MEM_BYTES ? total_mem : MAX_ITEM_MEM_BYTES;
}

/**
 * @brief Advanced CIP with Post-Retrieval for Equihash (144,5)
 *
 * This algorithm uses a configurable switching height to trade memory for computation:
 * - Forward pass Part 1 (non-indexed): L0 -> L1 -> ... -> L{h} using Item types (no index)
 * - Transition: expand_layer_to_idx_inplace converts Layer{h} to Layer{h}_IDX in-place
 * - Forward pass Part 2 (indexed): L{h}_IDX -> ... -> L{K-1}_IDX -> IP_K, storing IP{h+1}...IP{K-1}
 * - Backward pass: Expand solutions using stored IP layers (IP_K, IP{K-1}, ..., IP{h+1})
 * - Post-retrieval: Recover IP{h}, ..., IP1 on-demand by re-running the forward pass
 *
 * Memory layout optimization (for switching_height h=1..K-2, K=5):
 * ┌────────────────────────────────────────────────────────────────────┐
 * │ Layer Buffer (reused)           │ IP{K-1} │ IP{K-2} │...│ IP{h+1} │
 * │ [0, Layer3_IDX_size)            │         │         │   │         │
 * └────────────────────────────────────────────────────────────────────┘
 * ^base                                                     ↑         ^base_end
 *                                         First IP generated at end ──┘
 *
 * Key insights:
 * 1. Layer buffer reuse: All layers (both Item and ItemIDX types) use [0, Layer3_IDX_size)
 *    - Part 1: Item0...Item{h} are all smaller than Layer3_IDX (13 bytes vs 22,19,16,13)
 *    - Part 2: Layer{h}_IDX...Layer{K-1}_IDX shrink monotonically (e.g., 16→13→10 bytes)
 *    - Peak: Layer3_IDX at 13+4=17 bytes per item (not Layer0_IDX at 26 bytes)
 * 
 * 2. IP storage allocation (backwards from buffer end):
 *    - Stores (K-1-h) IP arrays: IP{h+1}, IP{h+2}, ..., IP{K-1}
 *    - First generated IP{h+1} placed at rightmost: base_end - (K-1-h)*MAX_IP_MEM_BYTES
 *    - Last generated IP{K-1} placed at leftmost: base_end - 1*MAX_IP_MEM_BYTES
 *    - Example (h=1): IP2 at base_end-3*MAX_IP, IP3 at base_end-2*MAX_IP, IP4 at base_end-1*MAX_IP
 * 
 * 3. IP5 reuses layer buffer: After L4_IDX is cleared, IP5 occupies [0, MAX_IP_MEM_BYTES)
 * 
 * 4. Memory lower bound: max(Layer3_IDX_size + IP_storage, MAX_ITEM_MEM_BYTES)
 *    - Ensures recover_IP() has sufficient space (may use Layer0_IDX for h=1 recovery)
 *
 * @tparam switching_height The layer at which to start tracking indices (0 to K-1)
 *   - 0: plain_cip (stores all IP1-IP4, max memory ~1.8GB)
 *   - 1-3: hybrid (memory/time tradeoff)
 *   - 4: plain_cip_pr (recovers all, min memory ~750MB, max time)
 */
template <int switching_height>
std::vector<Solution> advanced_cip_pr(int seed, uint8_t *base = nullptr)
{
    constexpr int K = EquihashParams::K; // K = 9
    static_assert(switching_height >= 0 && switching_height < K, "Invalid switching height");

     if constexpr (switching_height == 0)
    {
        // Allocate with peak_memory upfront (same pattern as other branches)
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024)
                      << " (h=0)" << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        Layer_IP IP1 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP2 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP3 = init_layer<Item_IP>(base_end - 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP4 = init_layer<Item_IP>(base_end - 4 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);

        Layer_IP IP5 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        fill_layer0(L0, seed);

        Layer0_IDX L0_IDX = expand_layer_to_idx_inplace<Item0, Item0_IDX>(L0);

        Layer1_IDX L1_IDX = init_layer<Item1_IDX>(base, MAX_LIST_SIZE * sizeof(Item1_IDX));
        merge0_ip_inplace(L0_IDX, L1_IDX, IP1);
        set_index_batch(L1_IDX);
        clear_vec(L0_IDX);

        Layer2_IDX L2_IDX = init_layer<Item2_IDX>(base, MAX_LIST_SIZE * sizeof(Item2_IDX));
        merge1_ip_inplace(L1_IDX, L2_IDX, IP2);
        set_index_batch(L2_IDX);
        clear_vec(L1_IDX);

        Layer3_IDX L3_IDX = init_layer<Item3_IDX>(base, MAX_LIST_SIZE * sizeof(Item3_IDX));
        merge2_ip_inplace(L2_IDX, L3_IDX, IP3);
        set_index_batch(L3_IDX);
        clear_vec(L2_IDX);

        Layer4_IDX L4_IDX = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
        merge3_ip_inplace(L3_IDX, L4_IDX, IP4);
        set_index_batch(L4_IDX);
        clear_vec(L3_IDX);

        merge4_inplace_for_ip(L4_IDX, IP5);
        clear_vec(L4_IDX);

        IFV
        {
            std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
            std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
            std::cout << "Layer 3 IP size: " << IP3.size() << std::endl;
            std::cout << "Layer 2 IP size: " << IP2.size() << std::endl;
            std::cout << "Layer 1 IP size: " << IP1.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP5.empty())
        {
            expand_solutions(solutions, IP5);
            expand_solutions(solutions, IP4);
            expand_solutions(solutions, IP3);
            expand_solutions(solutions, IP2);
            expand_solutions(solutions, IP1);

            filter_trivial_solutions(solutions);
        }

        if (own_base)
        {
            std::free(base);
        }
        return solutions;
    }
    else if constexpr (switching_height == K - 1)
    {
        return plain_cip_pr(seed, base);
    }
    else if constexpr (switching_height == 1)
    {
        // switching_height = 1: store IP2, IP3, IP4 (3 layers), recover IP1
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        // IP storage allocation (backwards from buffer end):
        // First generated IP2 at rightmost: base_end - 1*MAX_IP_MEM_BYTES
        // Then IP3 in middle: base_end - 2*MAX_IP_MEM_BYTES
        // Last generated IP4 at leftmost: base_end - 3*MAX_IP_MEM_BYTES
        Layer_IP IP2 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP3 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP4 = init_layer<Item_IP>(base_end - 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP5 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES); // IP5 reuses layer buffer at front

        // Forward pass Part 1: L0 -> L1 (non-indexed)
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
        fill_layer0(L0, seed);
        merge0_inplace(L0, L1);
        clear_vec(L0);

        // Transition: Add indices at layer 1
        Layer1_IDX L1_IDX = expand_layer_to_idx_inplace<Item1, Item1_IDX>(L1);

        // Forward pass Part 2: Indexed layers with IP storage
        Layer2_IDX L2_IDX = init_layer<Item2_IDX>(base, MAX_LIST_SIZE * sizeof(Item2_IDX));
        merge1_ip_inplace(L1_IDX, L2_IDX, IP2);
        set_index_batch(L2_IDX);
        clear_vec(L1_IDX);

        Layer3_IDX L3_IDX = init_layer<Item3_IDX>(base, MAX_LIST_SIZE * sizeof(Item3_IDX));
        merge2_ip_inplace(L2_IDX, L3_IDX, IP3);
        set_index_batch(L3_IDX);
        clear_vec(L2_IDX);

        Layer4_IDX L4_IDX = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
        merge3_ip_inplace(L3_IDX, L4_IDX, IP4);
        set_index_batch(L4_IDX);
        clear_vec(L3_IDX);

        merge4_inplace_for_ip(L4_IDX, IP5);
        clear_vec(L4_IDX);

        IFV
        {
            std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
            std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
            std::cout << "Layer 3 IP size: " << IP3.size() << std::endl;
            std::cout << "Layer 2 IP size: " << IP2.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP5.empty())
        {
            expand_solutions(solutions, IP5);
            expand_solutions(solutions, IP4);
            expand_solutions(solutions, IP3);
            expand_solutions(solutions, IP2);

            // Recover and expand IP1 on-demand
            Layer_IP IP1 = recover_IP(1, seed, base);
            IFV { std::cout << "Layer 1 IP size: " << IP1.size() << std::endl; }
            expand_solutions(solutions, IP1);
            filter_trivial_solutions(solutions);
        }

        if (own_base)
        {
            std::free(base);
        }
        return solutions;
    }
    else if constexpr (switching_height == 2)
    {
        // switching_height = 2: store IP3, IP4 (2 layers), recover IP1, IP2
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        // IP storage allocation (backwards from buffer end):
        // First generated IP3 at rightmost: base_end - 1*MAX_IP_MEM_BYTES
        // Last generated IP4 at leftmost: base_end - 2*MAX_IP_MEM_BYTES
        Layer_IP IP3 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP4 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP5 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES); // IP5 reuses layer buffer at front

        // Forward pass Part 1: L0 -> L1 -> L2 (non-indexed)
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
        Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
        fill_layer0(L0, seed);
        merge0_inplace(L0, L1);
        clear_vec(L0);
        merge1_inplace(L1, L2);
        clear_vec(L1);

        // Transition: Add indices at layer 2
        Layer2_IDX L2_IDX = expand_layer_to_idx_inplace<Item2, Item2_IDX>(L2);

        // Forward pass Part 2: Indexed layers with IP storage
        Layer3_IDX L3_IDX = init_layer<Item3_IDX>(base, MAX_LIST_SIZE * sizeof(Item3_IDX));
        merge2_ip_inplace(L2_IDX, L3_IDX, IP3);
        set_index_batch(L3_IDX);
        clear_vec(L2_IDX);

        Layer4_IDX L4_IDX = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
        merge3_ip_inplace(L3_IDX, L4_IDX, IP4);
        set_index_batch(L4_IDX);
        clear_vec(L3_IDX);

        merge4_inplace_for_ip(L4_IDX, IP5);
        clear_vec(L4_IDX);

        IFV
        {
            std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
            std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
            std::cout << "Layer 3 IP size: " << IP3.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP5.empty())
        {
            expand_solutions(solutions, IP5);
            expand_solutions(solutions, IP4);
            expand_solutions(solutions, IP3);

            // Recover and expand IP2, IP1 on-demand
            for (int h = 2; h >= 1; --h)
            {
                Layer_IP IPh = recover_IP(h, seed, base);
                IFV { std::cout << "Layer " << h << " IP size: " << IPh.size() << std::endl; }
                expand_solutions(solutions, IPh);
            }
            filter_trivial_solutions(solutions);
        }

        if (own_base)
        {
            std::free(base);
        }
        return solutions;
    }
    else if constexpr (switching_height == 3)
    {
        // switching_height = 3: store IP4 (1 layer), recover IP1, IP2, IP3
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        // IP storage allocation (backwards from buffer end):
        // Only IP4 at rightmost: base_end - 1*MAX_IP_MEM_BYTES
        Layer_IP IP4 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP5 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES); // IP5 reuses layer buffer at front

        // Forward pass Part 1: L0 -> L1 -> L2 -> L3 (non-indexed)
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
        Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
        Layer3 L3 = init_layer<Item3>(base, MAX_LIST_SIZE * sizeof(Item3));
        fill_layer0(L0, seed);
        merge0_inplace(L0, L1);
        clear_vec(L0);
        merge1_inplace(L1, L2);
        clear_vec(L1);
        merge2_inplace(L2, L3);
        clear_vec(L2);

        // Transition: Add indices at layer 3
        Layer3_IDX L3_IDX = expand_layer_to_idx_inplace<Item3, Item3_IDX>(L3);

        // Forward pass Part 2: Indexed layers with IP storage
        Layer4_IDX L4_IDX = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
        merge3_ip_inplace(L3_IDX, L4_IDX, IP4);
        set_index_batch(L4_IDX);
        clear_vec(L3_IDX);

        merge4_inplace_for_ip(L4_IDX, IP5);
        clear_vec(L4_IDX);

        IFV
        {
            std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
            std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP5.empty())
        {
            expand_solutions(solutions, IP5);
            expand_solutions(solutions, IP4);

            // Recover and expand IP3, IP2, IP1 on-demand
            for (int h = 3; h >= 1; --h)
            {
                Layer_IP IPh = recover_IP(h, seed, base);
                IFV { std::cout << "Layer " << h << " IP size: " << IPh.size() << std::endl; }
                expand_solutions(solutions, IPh);
            }
            filter_trivial_solutions(solutions);
        }

        if (own_base)
        {
            std::free(base);
        }
        return solutions;
    }
}

// Explicit instantiation and dispatcher
template std::vector<Solution> advanced_cip_pr<0>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<1>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<2>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<3>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<4>(int, uint8_t *);

std::vector<Solution> run_advanced_cip_pr(int seed, int h, uint8_t *base)
{
    switch (h)
    {
    case 0:
        return advanced_cip_pr<0>(seed, base);
    case 1:
        return advanced_cip_pr<1>(seed, base);
    case 2:
        return advanced_cip_pr<2>(seed, base);
    case 3:
        return advanced_cip_pr<3>(seed, base);
    case 4:
        return advanced_cip_pr<4>(seed, base);
    default:
        std::cerr << "Unsupported switching height: " << h << " (must be 0-4 for Equihash 144,5)" << std::endl;
        return {};
    }
}
