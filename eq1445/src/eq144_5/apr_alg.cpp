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
    const size_t total_mem = MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES * 4;
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
