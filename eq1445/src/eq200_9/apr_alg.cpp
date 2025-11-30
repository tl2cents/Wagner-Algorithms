#include "eq200_9/equihash_200_9.h"
#include "eq200_9/merge_200_9.h"
#include "eq200_9/sort_200_9.h"
#include "eq200_9/util_200_9.h"

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

const size_t ItemSizes[9] = {
    EquihashParams::kLayer0XorBytes,
    EquihashParams::kLayer1XorBytes,
    EquihashParams::kLayer2XorBytes,
    EquihashParams::kLayer3XorBytes,
    EquihashParams::kLayer4XorBytes,
    EquihashParams::kLayer5XorBytes,
    EquihashParams::kLayer6XorBytes,
    EquihashParams::kLayer7XorBytes,
    EquihashParams::kLayer8XorBytes};

const size_t ItemIDXSizes[9] = {
    EquihashParams::kLayer0XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer1XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer2XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer3XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer4XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer5XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer6XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer7XorBytes + EquihashParams::kIndexBytes,
    EquihashParams::kLayer8XorBytes + EquihashParams::kIndexBytes};

inline Layer_IP recover_IP(int h, int seed, uint8_t *base)
{
    assert(h >= 1 && h <= 9 && "recover_IP only supports layers 1-9 for (200,9)");

    Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
    Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
    Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
    Layer3 L3 = init_layer<Item3>(base, MAX_LIST_SIZE * sizeof(Item3));
    Layer4 L4 = init_layer<Item4>(base, MAX_LIST_SIZE * sizeof(Item4));
    Layer5 L5 = init_layer<Item5>(base, MAX_LIST_SIZE * sizeof(Item5));
    Layer6 L6 = init_layer<Item6>(base, MAX_LIST_SIZE * sizeof(Item6));
    Layer7 L7 = init_layer<Item7>(base, MAX_LIST_SIZE * sizeof(Item7));
    Layer8 L8 = init_layer<Item8>(base, MAX_LIST_SIZE * sizeof(Item8));
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
    if (h == 5)
    {
        Layer4_IDX L4_IDX = expand_layer_to_idx_inplace<Item4, Item4_IDX>(L4);
        merge4_inplace_for_ip(L4_IDX, out_IP);
        clear_vec(L4_IDX);
        return out_IP;
    }

    merge4_inplace(L4, L5);
    clear_vec(L4);
    if (h == 6)
    {
        Layer5_IDX L5_IDX = expand_layer_to_idx_inplace<Item5, Item5_IDX>(L5);
        merge5_inplace_for_ip(L5_IDX, out_IP);
        clear_vec(L5_IDX);
        return out_IP;
    }

    merge5_inplace(L5, L6);
    clear_vec(L5);
    if (h == 7)
    {
        Layer6_IDX L6_IDX = expand_layer_to_idx_inplace<Item6, Item6_IDX>(L6);
        merge6_inplace_for_ip(L6_IDX, out_IP);
        clear_vec(L6_IDX);
        return out_IP;
    }

    merge6_inplace(L6, L7);
    clear_vec(L6);
    if (h == 8)
    {
        Layer7_IDX L7_IDX = expand_layer_to_idx_inplace<Item7, Item7_IDX>(L7);
        merge7_inplace_for_ip(L7_IDX, out_IP);
        clear_vec(L7_IDX);
        return out_IP;
    }

    merge7_inplace(L7, L8);
    clear_vec(L7);
    Layer8_IDX L8_IDX = expand_layer_to_idx_inplace<Item8, Item8_IDX>(L8);
    merge8_inplace_for_ip(L8_IDX, out_IP);
    clear_vec(L8_IDX);
    return out_IP;
}

std::vector<Solution> plain_cip(int seed, uint8_t *base)
{
    const size_t total_mem = MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES * 8; // K-1 = 8
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
    Layer5_IDX L5 = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
    Layer6_IDX L6 = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
    Layer7_IDX L7 = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
    Layer8_IDX L8 = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));

    Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);
    Layer_IP IP8 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 0 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP7 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP6 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP5 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP4 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 4 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP3 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 5 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP2 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 6 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP1 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 7 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);

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

    merge4_ip_inplace(L4, L5, IP5);
    set_index_batch(L5);
    clear_vec(L4);

    merge5_ip_inplace(L5, L6, IP6);
    set_index_batch(L6);
    clear_vec(L5);

    merge6_ip_inplace(L6, L7, IP7);
    set_index_batch(L7);
    clear_vec(L6);

    merge7_ip_inplace(L7, L8, IP8);
    set_index_batch(L8);
    clear_vec(L7);

    merge8_inplace_for_ip(L8, IP9);
    clear_vec(L8);

    IFV
    {
        std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
        std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
        std::cout << "Layer 7 IP size: " << IP7.size() << std::endl;
        std::cout << "Layer 6 IP size: " << IP6.size() << std::endl;
        std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
        std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
        std::cout << "Layer 3 IP size: " << IP3.size() << std::endl;
        std::cout << "Layer 2 IP size: " << IP2.size() << std::endl;
        std::cout << "Layer 1 IP size: " << IP1.size() << std::endl;
    }

    if (!IP9.empty())
    {
        expand_solutions(solutions, IP9);
        expand_solutions(solutions, IP8);
        expand_solutions(solutions, IP7);
        expand_solutions(solutions, IP6);
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
    Layer_IP IP9 = recover_IP(9, seed, base);
    IFV { std::cout << "Layer 9 IP size: " << IP9.size() << std::endl; }

    if (!IP9.empty())
    {
        expand_solutions(solutions, IP9);
        for (int h = 8; h >= 1; --h)
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
    Layer5_IDX L5 = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
    Layer6_IDX L6 = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
    Layer7_IDX L7 = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
    Layer8_IDX L8 = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
    Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

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
    manifest.ip[4].offset = writer.get_current_offset();
    set_index_batch(L4);
    clear_vec(L3);
    IFV { std::cout << "Layer 4 size: " << L4.size() << std::endl; }

    merge4_em_ip_inplace(L4, L5, writer);
    manifest.ip[4].count = L5.size();
    manifest.ip[5].offset = writer.get_current_offset();
    set_index_batch(L5);
    clear_vec(L4);
    IFV { std::cout << "Layer 5 size: " << L5.size() << std::endl; }

    merge5_em_ip_inplace(L5, L6, writer);
    manifest.ip[5].count = L6.size();
    manifest.ip[6].offset = writer.get_current_offset();
    set_index_batch(L6);
    clear_vec(L5);
    IFV { std::cout << "Layer 6 size: " << L6.size() << std::endl; }

    merge6_em_ip_inplace(L6, L7, writer);
    manifest.ip[6].count = L7.size();
    manifest.ip[7].offset = writer.get_current_offset();
    set_index_batch(L7);
    clear_vec(L6);
    IFV { std::cout << "Layer 7 size: " << L7.size() << std::endl; }

    merge7_em_ip_inplace(L7, L8, writer);
    manifest.ip[7].count = L8.size();
    set_index_batch(L8);
    clear_vec(L7);
    IFV { std::cout << "Layer 8 size: " << L8.size() << std::endl; }

    merge8_inplace_for_ip(L8, IP9);
    clear_vec(L8);

    writer.close();

    std::vector<Solution> solutions;
    if (!IP9.empty())
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

        expand_solutions(solutions, IP9);
        for (int i = 7; i >= 0; --i)
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
 * @brief Advanced CIP with Post-Retrieval for Equihash (200,9)
 *
 * switching_height meanings:
 * - 0: plain_cip (full forward, store IP1..IP8)
 * - 1..7: hybrid, store IP{h+1..8} and recover IP{h..1}
 * - 8: plain_cip_pr (full post-retrieval, recover IP9..IP1)
 */
template <int switching_height>
std::vector<Solution> advanced_cip_pr(int seed, uint8_t *base /* = nullptr */)
{
    constexpr int K = EquihashParams::K; // K = 9
    static_assert(switching_height >= 0 && switching_height < K, "Invalid switching height");

    // ====== h = 0: full forward + store IP1..IP8 using the advanced layout (not plain_cip) ======
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

        // IP storage: place IP1..IP8 from the end backward
        Layer_IP IP1 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP2 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP3 = init_layer<Item_IP>(base_end - 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP4 = init_layer<Item_IP>(base_end - 4 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP5 = init_layer<Item_IP>(base_end - 5 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP6 = init_layer<Item_IP>(base_end - 6 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP7 = init_layer<Item_IP>(base_end - 7 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP8 = init_layer<Item_IP>(base_end - 8 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);

        // IP9 reuses the front of the buffer (same as other branches)
        Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

        // Part 1: non-indexed; only up to Layer0 (since h=0)
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        fill_layer0(L0, seed);

        // Transition: L0 -> L0_IDX, attach indices here
        Layer0_IDX L0_IDX = expand_layer_to_idx_inplace<Item0, Item0_IDX>(L0);

        // Part 2: fully indexed forward, record IP1..IP8

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

        Layer5_IDX L5_IDX = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
        merge4_ip_inplace(L4_IDX, L5_IDX, IP5);
        set_index_batch(L5_IDX);
        clear_vec(L4_IDX);

        Layer6_IDX L6_IDX = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
        merge5_ip_inplace(L5_IDX, L6_IDX, IP6);
        set_index_batch(L6_IDX);
        clear_vec(L5_IDX);

        Layer7_IDX L7_IDX = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
        merge6_ip_inplace(L6_IDX, L7_IDX, IP7);
        set_index_batch(L7_IDX);
        clear_vec(L6_IDX);

        Layer8_IDX L8_IDX = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
        merge7_ip_inplace(L7_IDX, L8_IDX, IP8);
        set_index_batch(L8_IDX);
        clear_vec(L7_IDX);

        // Last layer: L8_IDX -> IP9 (no parent IP recorded for IP9)
        merge8_inplace_for_ip(L8_IDX, IP9);
        clear_vec(L8_IDX);

        IFV
        {
            std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
            std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
            std::cout << "Layer 7 IP size: " << IP7.size() << std::endl;
            std::cout << "Layer 6 IP size: " << IP6.size() << std::endl;
            std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
            std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
            std::cout << "Layer 3 IP size: " << IP3.size() << std::endl;
            std::cout << "Layer 2 IP size: " << IP2.size() << std::endl;
            std::cout << "Layer 1 IP size: " << IP1.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP9.empty())
        {
            // h=0 skips post-retrieval; expand directly from stored IP1..IP9
            expand_solutions(solutions, IP9);
            expand_solutions(solutions, IP8);
            expand_solutions(solutions, IP7);
            expand_solutions(solutions, IP6);
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
        // Store IP2..IP8 (7 layers), recover IP1
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << " (h=1)" << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        // IP storage: place IP2..IP8 from buffer end backward
        Layer_IP IP2 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP3 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP4 = init_layer<Item_IP>(base_end - 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP5 = init_layer<Item_IP>(base_end - 4 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP6 = init_layer<Item_IP>(base_end - 5 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP7 = init_layer<Item_IP>(base_end - 6 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP8 = init_layer<Item_IP>(base_end - 7 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES); // IP9 reuses the front buffer

        // Part 1: L0 -> L1 (non-indexed)
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
        fill_layer0(L0, seed);
        merge0_inplace(L0, L1);
        clear_vec(L0);

        // Transition: L1 -> L1_IDX
        Layer1_IDX L1_IDX = expand_layer_to_idx_inplace<Item1, Item1_IDX>(L1);

        // Part 2: indexed + record IP2..IP8
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

        Layer5_IDX L5_IDX = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
        merge4_ip_inplace(L4_IDX, L5_IDX, IP5);
        set_index_batch(L5_IDX);
        clear_vec(L4_IDX);

        Layer6_IDX L6_IDX = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
        merge5_ip_inplace(L5_IDX, L6_IDX, IP6);
        set_index_batch(L6_IDX);
        clear_vec(L5_IDX);

        Layer7_IDX L7_IDX = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
        merge6_ip_inplace(L6_IDX, L7_IDX, IP7);
        set_index_batch(L7_IDX);
        clear_vec(L6_IDX);

        Layer8_IDX L8_IDX = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
        merge7_ip_inplace(L7_IDX, L8_IDX, IP8);
        set_index_batch(L8_IDX);
        clear_vec(L7_IDX);

        merge8_inplace_for_ip(L8_IDX, IP9);
        clear_vec(L8_IDX);

        IFV
        {
            std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
            std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
            std::cout << "Layer 7 IP size: " << IP7.size() << std::endl;
            std::cout << "Layer 6 IP size: " << IP6.size() << std::endl;
            std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
            std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
            std::cout << "Layer 3 IP size: " << IP3.size() << std::endl;
            std::cout << "Layer 2 IP size: " << IP2.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP9.empty())
        {
            expand_solutions(solutions, IP9);
            expand_solutions(solutions, IP8);
            expand_solutions(solutions, IP7);
            expand_solutions(solutions, IP6);
            expand_solutions(solutions, IP5);
            expand_solutions(solutions, IP4);
            expand_solutions(solutions, IP3);
            expand_solutions(solutions, IP2);

            // Recover IP1
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
        // Store IP3..IP8 (6 layers), recover IP1..IP2
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << " (h=2)" << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        Layer_IP IP3 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP4 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP5 = init_layer<Item_IP>(base_end - 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP6 = init_layer<Item_IP>(base_end - 4 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP7 = init_layer<Item_IP>(base_end - 5 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP8 = init_layer<Item_IP>(base_end - 6 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

        // Part 1: L0 -> L1 -> L2
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
        Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
        fill_layer0(L0, seed);
        merge0_inplace(L0, L1);
        clear_vec(L0);
        merge1_inplace(L1, L2);
        clear_vec(L1);

        // Transition: L2 -> L2_IDX
        Layer2_IDX L2_IDX = expand_layer_to_idx_inplace<Item2, Item2_IDX>(L2);

        // Part 2
        Layer3_IDX L3_IDX = init_layer<Item3_IDX>(base, MAX_LIST_SIZE * sizeof(Item3_IDX));
        merge2_ip_inplace(L2_IDX, L3_IDX, IP3);
        set_index_batch(L3_IDX);
        clear_vec(L2_IDX);

        Layer4_IDX L4_IDX = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
        merge3_ip_inplace(L3_IDX, L4_IDX, IP4);
        set_index_batch(L4_IDX);
        clear_vec(L3_IDX);

        Layer5_IDX L5_IDX = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
        merge4_ip_inplace(L4_IDX, L5_IDX, IP5);
        set_index_batch(L5_IDX);
        clear_vec(L4_IDX);

        Layer6_IDX L6_IDX = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
        merge5_ip_inplace(L5_IDX, L6_IDX, IP6);
        set_index_batch(L6_IDX);
        clear_vec(L5_IDX);

        Layer7_IDX L7_IDX = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
        merge6_ip_inplace(L6_IDX, L7_IDX, IP7);
        set_index_batch(L7_IDX);
        clear_vec(L6_IDX);

        Layer8_IDX L8_IDX = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
        merge7_ip_inplace(L7_IDX, L8_IDX, IP8);
        set_index_batch(L8_IDX);
        clear_vec(L7_IDX);

        merge8_inplace_for_ip(L8_IDX, IP9);
        clear_vec(L8_IDX);

        IFV
        {
            std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
            std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
            std::cout << "Layer 7 IP size: " << IP7.size() << std::endl;
            std::cout << "Layer 6 IP size: " << IP6.size() << std::endl;
            std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
            std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
            std::cout << "Layer 3 IP size: " << IP3.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP9.empty())
        {
            expand_solutions(solutions, IP9);
            expand_solutions(solutions, IP8);
            expand_solutions(solutions, IP7);
            expand_solutions(solutions, IP6);
            expand_solutions(solutions, IP5);
            expand_solutions(solutions, IP4);
            expand_solutions(solutions, IP3);

            // Recover IP2, IP1
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
        // Store IP4..IP8 (5 layers), recover IP1..IP3
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << " (h=3)" << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        Layer_IP IP4 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP5 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP6 = init_layer<Item_IP>(base_end - 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP7 = init_layer<Item_IP>(base_end - 4 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP8 = init_layer<Item_IP>(base_end - 5 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

        // Part 1: L0 -> L1 -> L2 -> L3
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

        // Transition: L3 -> L3_IDX
        Layer3_IDX L3_IDX = expand_layer_to_idx_inplace<Item3, Item3_IDX>(L3);

        // Part 2
        Layer4_IDX L4_IDX = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
        merge3_ip_inplace(L3_IDX, L4_IDX, IP4);
        set_index_batch(L4_IDX);
        clear_vec(L3_IDX);

        Layer5_IDX L5_IDX = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
        merge4_ip_inplace(L4_IDX, L5_IDX, IP5);
        set_index_batch(L5_IDX);
        clear_vec(L4_IDX);

        Layer6_IDX L6_IDX = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
        merge5_ip_inplace(L5_IDX, L6_IDX, IP6);
        set_index_batch(L6_IDX);
        clear_vec(L5_IDX);

        Layer7_IDX L7_IDX = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
        merge6_ip_inplace(L6_IDX, L7_IDX, IP7);
        set_index_batch(L7_IDX);
        clear_vec(L6_IDX);

        Layer8_IDX L8_IDX = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
        merge7_ip_inplace(L7_IDX, L8_IDX, IP8);
        set_index_batch(L8_IDX);
        clear_vec(L7_IDX);

        merge8_inplace_for_ip(L8_IDX, IP9);
        clear_vec(L8_IDX);

        IFV
        {
            std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
            std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
            std::cout << "Layer 7 IP size: " << IP7.size() << std::endl;
            std::cout << "Layer 6 IP size: " << IP6.size() << std::endl;
            std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
            std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP9.empty())
        {
            expand_solutions(solutions, IP9);
            expand_solutions(solutions, IP8);
            expand_solutions(solutions, IP7);
            expand_solutions(solutions, IP6);
            expand_solutions(solutions, IP5);
            expand_solutions(solutions, IP4);

            // Recover IP3, IP2, IP1
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
    else if constexpr (switching_height == 4)
    {
        // Store IP5..IP8 (4 layers), recover IP1..IP4
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << " (h=4)" << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        Layer_IP IP5 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP6 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP7 = init_layer<Item_IP>(base_end - 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP8 = init_layer<Item_IP>(base_end - 4 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

        // Part 1: L0 -> L1 -> L2 -> L3 -> L4
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
        Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
        Layer3 L3 = init_layer<Item3>(base, MAX_LIST_SIZE * sizeof(Item3));
        Layer4 L4 = init_layer<Item4>(base, MAX_LIST_SIZE * sizeof(Item4));
        fill_layer0(L0, seed);
        merge0_inplace(L0, L1);
        clear_vec(L0);
        merge1_inplace(L1, L2);
        clear_vec(L1);
        merge2_inplace(L2, L3);
        clear_vec(L2);
        merge3_inplace(L3, L4);
        clear_vec(L3);

        // Transition: L4 -> L4_IDX
        Layer4_IDX L4_IDX = expand_layer_to_idx_inplace<Item4, Item4_IDX>(L4);

        // Part 2
        Layer5_IDX L5_IDX = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
        merge4_ip_inplace(L4_IDX, L5_IDX, IP5);
        set_index_batch(L5_IDX);
        clear_vec(L4_IDX);

        Layer6_IDX L6_IDX = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
        merge5_ip_inplace(L5_IDX, L6_IDX, IP6);
        set_index_batch(L6_IDX);
        clear_vec(L5_IDX);

        Layer7_IDX L7_IDX = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
        merge6_ip_inplace(L6_IDX, L7_IDX, IP7);
        set_index_batch(L7_IDX);
        clear_vec(L6_IDX);

        Layer8_IDX L8_IDX = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
        merge7_ip_inplace(L7_IDX, L8_IDX, IP8);
        set_index_batch(L8_IDX);
        clear_vec(L7_IDX);

        merge8_inplace_for_ip(L8_IDX, IP9);
        clear_vec(L8_IDX);

        IFV
        {
            std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
            std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
            std::cout << "Layer 7 IP size: " << IP7.size() << std::endl;
            std::cout << "Layer 6 IP size: " << IP6.size() << std::endl;
            std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP9.empty())
        {
            expand_solutions(solutions, IP9);
            expand_solutions(solutions, IP8);
            expand_solutions(solutions, IP7);
            expand_solutions(solutions, IP6);
            expand_solutions(solutions, IP5);

            // Recover IP4..IP1
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
    else if constexpr (switching_height == 5)
    {
        // Store IP6..IP8 (3 layers), recover IP1..IP5
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << " (h=5)" << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        Layer_IP IP6 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP7 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP8 = init_layer<Item_IP>(base_end - 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

        // Part 1: L0 -> L1 -> L2 -> L3 -> L4 -> L5
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
        Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
        Layer3 L3 = init_layer<Item3>(base, MAX_LIST_SIZE * sizeof(Item3));
        Layer4 L4 = init_layer<Item4>(base, MAX_LIST_SIZE * sizeof(Item4));
        Layer5 L5 = init_layer<Item5>(base, MAX_LIST_SIZE * sizeof(Item5));
        fill_layer0(L0, seed);
        merge0_inplace(L0, L1);
        clear_vec(L0);
        merge1_inplace(L1, L2);
        clear_vec(L1);
        merge2_inplace(L2, L3);
        clear_vec(L2);
        merge3_inplace(L3, L4);
        clear_vec(L3);
        merge4_inplace(L4, L5);
        clear_vec(L4);

        // Transition: L5 -> L5_IDX
        Layer5_IDX L5_IDX = expand_layer_to_idx_inplace<Item5, Item5_IDX>(L5);

        // Part 2
        Layer6_IDX L6_IDX = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
        merge5_ip_inplace(L5_IDX, L6_IDX, IP6);
        set_index_batch(L6_IDX);
        clear_vec(L5_IDX);

        Layer7_IDX L7_IDX = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
        merge6_ip_inplace(L6_IDX, L7_IDX, IP7);
        set_index_batch(L7_IDX);
        clear_vec(L6_IDX);

        Layer8_IDX L8_IDX = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
        merge7_ip_inplace(L7_IDX, L8_IDX, IP8);
        set_index_batch(L8_IDX);
        clear_vec(L7_IDX);

        merge8_inplace_for_ip(L8_IDX, IP9);
        clear_vec(L8_IDX);

        IFV
        {
            std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
            std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
            std::cout << "Layer 7 IP size: " << IP7.size() << std::endl;
            std::cout << "Layer 6 IP size: " << IP6.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP9.empty())
        {
            expand_solutions(solutions, IP9);
            expand_solutions(solutions, IP8);
            expand_solutions(solutions, IP7);
            expand_solutions(solutions, IP6);

            // Recover IP5..IP1
            for (int h = 5; h >= 1; --h)
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
    else if constexpr (switching_height == 6)
    {
        // Store IP7..IP8 (2 layers), recover IP1..IP6
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << " (h=6)" << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        Layer_IP IP7 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP8 = init_layer<Item_IP>(base_end - 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

        // Part 1: L0 -> L1 -> L2 -> L3 -> L4 -> L5 -> L6
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
        Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
        Layer3 L3 = init_layer<Item3>(base, MAX_LIST_SIZE * sizeof(Item3));
        Layer4 L4 = init_layer<Item4>(base, MAX_LIST_SIZE * sizeof(Item4));
        Layer5 L5 = init_layer<Item5>(base, MAX_LIST_SIZE * sizeof(Item5));
        Layer6 L6 = init_layer<Item6>(base, MAX_LIST_SIZE * sizeof(Item6));
        fill_layer0(L0, seed);
        merge0_inplace(L0, L1);
        clear_vec(L0);
        merge1_inplace(L1, L2);
        clear_vec(L1);
        merge2_inplace(L2, L3);
        clear_vec(L2);
        merge3_inplace(L3, L4);
        clear_vec(L3);
        merge4_inplace(L4, L5);
        clear_vec(L4);
        merge5_inplace(L5, L6);
        clear_vec(L5);

        // Transition: L6 -> L6_IDX
        Layer6_IDX L6_IDX = expand_layer_to_idx_inplace<Item6, Item6_IDX>(L6);

        // Part 2
        Layer7_IDX L7_IDX = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
        merge6_ip_inplace(L6_IDX, L7_IDX, IP7);
        set_index_batch(L7_IDX);
        clear_vec(L6_IDX);

        Layer8_IDX L8_IDX = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
        merge7_ip_inplace(L7_IDX, L8_IDX, IP8);
        set_index_batch(L8_IDX);
        clear_vec(L7_IDX);

        merge8_inplace_for_ip(L8_IDX, IP9);
        clear_vec(L8_IDX);

        IFV
        {
            std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
            std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
            std::cout << "Layer 7 IP size: " << IP7.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP9.empty())
        {
            expand_solutions(solutions, IP9);
            expand_solutions(solutions, IP8);
            expand_solutions(solutions, IP7);

            // Recover IP6..IP1
            for (int h = 6; h >= 1; --h)
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
    else if constexpr (switching_height == 7)
    {
        // Store IP8 (1 layer), recover IP1..IP7
        size_t total_mem = advanced_cip_pr_peak_memory(switching_height);
        bool own_base = false;
        if (base == nullptr)
        {
            base = static_cast<uint8_t *>(std::malloc(total_mem));
            own_base = true;
        }
        IFV
        {
            std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << " (h=7)" << std::endl;
        }

        uint8_t *base_end = base + total_mem;

        Layer_IP IP8 = init_layer<Item_IP>(base_end - 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
        Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

        // Part 1: L0 -> L1 -> L2 -> L3 -> L4 -> L5 -> L6 -> L7
        Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
        Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
        Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
        Layer3 L3 = init_layer<Item3>(base, MAX_LIST_SIZE * sizeof(Item3));
        Layer4 L4 = init_layer<Item4>(base, MAX_LIST_SIZE * sizeof(Item4));
        Layer5 L5 = init_layer<Item5>(base, MAX_LIST_SIZE * sizeof(Item5));
        Layer6 L6 = init_layer<Item6>(base, MAX_LIST_SIZE * sizeof(Item6));
        Layer7 L7 = init_layer<Item7>(base, MAX_LIST_SIZE * sizeof(Item7));
        fill_layer0(L0, seed);
        merge0_inplace(L0, L1);
        clear_vec(L0);
        merge1_inplace(L1, L2);
        clear_vec(L1);
        merge2_inplace(L2, L3);
        clear_vec(L2);
        merge3_inplace(L3, L4);
        clear_vec(L3);
        merge4_inplace(L4, L5);
        clear_vec(L4);
        merge5_inplace(L5, L6);
        clear_vec(L5);
        merge6_inplace(L6, L7);
        clear_vec(L6);

        // Transition: L7 -> L7_IDX
        Layer7_IDX L7_IDX = expand_layer_to_idx_inplace<Item7, Item7_IDX>(L7);

        // Part 2
        Layer8_IDX L8_IDX = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
        merge7_ip_inplace(L7_IDX, L8_IDX, IP8);
        set_index_batch(L8_IDX);
        clear_vec(L7_IDX);

        merge8_inplace_for_ip(L8_IDX, IP9);
        clear_vec(L8_IDX);

        IFV
        {
            std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
            std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
        }

        std::vector<Solution> solutions;

        if (!IP9.empty())
        {
            expand_solutions(solutions, IP9);
            expand_solutions(solutions, IP8);

            // Recover IP7..IP1
            for (int h = 7; h >= 1; --h)
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
    // Should not reach here (cases 0..8 are covered)
    else
    {
        return {};
    }
}

// Explicit template instantiation
template std::vector<Solution> advanced_cip_pr<0>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<1>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<2>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<3>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<4>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<5>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<6>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<7>(int, uint8_t *);
template std::vector<Solution> advanced_cip_pr<8>(int, uint8_t *);

// Dispatcher: supports h=0..8
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
    case 5:
        return advanced_cip_pr<5>(seed, base);
    case 6:
        return advanced_cip_pr<6>(seed, base);
    case 7:
        return advanced_cip_pr<7>(seed, base);
    case 8:
        return advanced_cip_pr<8>(seed, base);
    default:
        std::cerr << "Unsupported switching height: " << h
                  << " (must be 0-8 for Equihash 200,9)" << std::endl;
        return {};
    }
}
