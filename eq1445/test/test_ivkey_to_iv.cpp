#include "eq144_5/equihash_144_5.h"
#include "eq144_5/merge_144_5.h"
#include "eq144_5/sort_144_5.h"
#include "core/util.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cassert>

using namespace std::chrono;

// Global variables required by sort.h
SortAlgo g_sort_algo = SortAlgo::KXSORT;
bool g_verbose = false;

// Helper to measure time
template<typename Func>
long long measure_ms(Func f) {
    auto start = high_resolution_clock::now();
    f();
    auto end = high_resolution_clock::now();
    return duration_cast<milliseconds>(end - start).count();
}

// Helper to compute IV hash generically
template <typename IVType>
inline Item0 compute_iv_hash_generic(int seed, const IVType &iv)
{
    Item0 acc{};
    std::memset(acc.XOR, 0, sizeof(acc.XOR));
    constexpr size_t kCount = IVType::NumIndices;
    for (size_t i = 0; i < kCount; ++i)
    {
        const size_t idx = iv.get_index(i);
        const Item0 leaf = compute_ith_item(seed, idx);
        for (size_t b = 0; b < sizeof(acc.XOR); ++b)
        {
            acc.XOR[b] ^= leaf.XOR[b];
        }
    }
    return acc;
}

// Helper to convert IV to XOR bytes (Item)
template<typename IVType, typename ItemType, size_t Layer>
std::vector<ItemType> iv_layer_to_items(const LayerVec<IVType>& iv_layer, int seed) {
    std::vector<ItemType> items;
    items.reserve(iv_layer.size());
    
    constexpr size_t shift_bytes = Layer * 3;
    
    for (const auto& iv : iv_layer) {
        Item0 full_xor = compute_iv_hash_generic(seed, iv);
        ItemType item;
        constexpr size_t item_size = sizeof(item.XOR);
        std::memcpy(item.XOR, full_xor.XOR + shift_bytes, item_size);
        items.push_back(item);
    }
    return items;
}

// Comparator for Items
template<typename ItemType>
struct ItemComparator {
    bool operator()(const ItemType& a, const ItemType& b) const {
        return std::memcmp(a.XOR, b.XOR, sizeof(a.XOR)) < 0;
    }
};

template<typename ItemType>
bool compare_layers(const std::vector<ItemType>& list1, const std::vector<ItemType>& list2, const char* label) {
    if (list1.size() != list2.size()) {
        std::cerr << label << ": Size mismatch! " << list1.size() << " vs " << list2.size() << std::endl;
        return false;
    }
    
    std::vector<ItemType> sorted1 = list1;
    std::vector<ItemType> sorted2 = list2;
    std::sort(sorted1.begin(), sorted1.end(), ItemComparator<ItemType>());
    std::sort(sorted2.begin(), sorted2.end(), ItemComparator<ItemType>());
    
    for (size_t i = 0; i < sorted1.size(); ++i) {
        if (std::memcmp(sorted1[i].XOR, sorted2[i].XOR, sizeof(sorted1[i].XOR)) != 0) {
            std::cerr << label << ": Mismatch at index " << i << std::endl;
            return false;
        }
    }
    std::cout << label << ": MATCH (" << list1.size() << " items)" << std::endl;
    return true;
}

int main() {
    int seed = 12345;
    std::cout << "Initializing Layer 0..." << std::endl;
    
    // Initialize Layer 0
    size_t layer0_bytes = EquihashParams::kLeafCountFull * sizeof(Item0);
    Layer0 layer0 = init_layer_with_new_memory<Item0>(layer0_bytes);
    fill_layer0(layer0, seed);
    
    // Create IV and IVKey versions
    size_t layer0_iv_bytes = EquihashParams::kLeafCountFull * sizeof(IV0);
    Layer0_IV layer0_iv = init_layer_with_new_memory<IV0>(layer0_iv_bytes);
    layer0_iv.resize(layer0.size());
    
    size_t layer0_ivkey_bytes = EquihashParams::kLeafCountFull * sizeof(IVKey0_24);
    Layer0_IVKey layer0_ivkey = init_layer_with_new_memory<IVKey0_24>(layer0_ivkey_bytes);
    layer0_ivkey.resize(layer0.size());
    
    for (size_t i = 0; i < layer0.size(); ++i) {
        layer0_iv[i].set_index(0, i);
        layer0_ivkey[i].set_index(0, i);
    }
    
    std::cout << "Layer 0 size: " << layer0.size() << std::endl;
    
    // -------------------------------------------------------------------------
    // Layer 0 -> Layer 1
    // -------------------------------------------------------------------------
    std::cout << "\n--- Testing Layer 0 -> Layer 1 (IVKey -> IV) ---" << std::endl;
    
    // 1. Baseline: merge0_iv_layer (Pure IV)
    size_t layer1_iv_bytes = MAX_LIST_SIZE * sizeof(IV1);
    Layer1_IV layer1_iv = init_layer_with_new_memory<IV1>(layer1_iv_bytes);
    
    long long t_iv = measure_ms([&]() {
        merge0_iv_layer(layer0_iv, layer1_iv, seed);
    });
    std::cout << "merge0_iv_layer (Baseline): " << t_iv << " ms, size=" << layer1_iv.size() << std::endl;
    
    // 2. New function: merge0_ivkey_for_iv_layer (IVKey -> IV)
    Layer1_IV layer1_iv_from_key = init_layer_with_new_memory<IV1>(layer1_iv_bytes);
    
    // Must compute keys first
    long long t_setkey = measure_ms([&]() {
        SetIV_Key_Batch<0, 24, uint32_t>(seed, layer0_ivkey);
    });
    
    long long t_new = measure_ms([&]() {
        merge0_ivkey_for_iv_layer(layer0_ivkey, layer1_iv_from_key, seed);
    });
    
    std::cout << "SetIV_Key: " << t_setkey << " ms" << std::endl;
    std::cout << "merge0_ivkey_for_iv_layer: " << t_new << " ms, size=" << layer1_iv_from_key.size() << std::endl;
    std::cout << "Total new time: " << (t_setkey + t_new) << " ms" << std::endl;
    
    // Verification
    std::cout << "Verifying results..." << std::endl;
    
    std::vector<Item1> items_base = iv_layer_to_items<IV1, Item1, 1>(layer1_iv, seed);
    std::vector<Item1> items_new = iv_layer_to_items<IV1, Item1, 1>(layer1_iv_from_key, seed);
    
    compare_layers(items_base, items_new, "Baseline vs New Function");
    
    return 0;
}
