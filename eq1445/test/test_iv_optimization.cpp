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
#include <set>

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

// Helper to compute IV hash generically (works for IV and IVKey)
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
// Layer 1: Item1 (15 bytes)
// Layer 2: Item2 (12 bytes)
template<typename IVType, typename ItemType, size_t Layer>
std::vector<ItemType> iv_layer_to_items(const LayerVec<IVType>& iv_layer, int seed) {
    std::vector<ItemType> items;
    items.reserve(iv_layer.size());
    
    // Calculate shift amount: Layer * 3 bytes (24 bits)
    constexpr size_t shift_bytes = Layer * 3;
    
    for (const auto& iv : iv_layer) {
        Item0 full_xor = compute_iv_hash_generic(seed, iv);
        ItemType item;
        
        // Verify that the first shift_bytes are zero
        for (size_t i = 0; i < shift_bytes; ++i) {
            if (full_xor.XOR[i] != 0) {
                std::cerr << "Error: IV hash has non-zero prefix at byte " << i 
                          << " value: " << (int)full_xor.XOR[i] << std::endl;
                // Don't abort, just log
            }
        }
        
        // Copy the remaining bytes to ItemType
        // ItemType::XOR size should be 18 - shift_bytes
        constexpr size_t item_size = sizeof(item.XOR);
        // assert(item_size == 18 - shift_bytes);
        
        std::memcpy(item.XOR, full_xor.XOR + shift_bytes, item_size);
        items.push_back(item);
    }
    return items;
}

// Comparator for Items to sort them for consistency check
template<typename ItemType>
struct ItemComparator {
    bool operator()(const ItemType& a, const ItemType& b) const {
        return std::memcmp(a.XOR, b.XOR, sizeof(a.XOR)) < 0;
    }
};

template<typename ItemType>
void verify_xor_zero(const LayerVec<ItemType>& layer, size_t zero_bytes) {
    for (size_t i = 0; i < layer.size(); ++i) {
        for (size_t j = 0; j < zero_bytes; ++j) {
            // Note: ItemType in LayerVec already has shifted XORs, so we check the *next* collision bits?
            // No, wait.
            // Layer 0 -> Layer 1 merge produces Item1.
            // Item1 has 15 bytes. The first 3 bytes of the *original* 18 bytes were zeroed out and removed.
            // So Item1[0..2] are the *next* 3 bytes of the hash.
            // The user requirement: "对于第一层的 IV，XOR[0,3] = 0"
            // This refers to the FULL XOR value (18 bytes).
            // But Item1 only stores the remaining 15 bytes.
            // So for Item1, we don't check for zeros, because the zeros are implicit (shifted out).
            // However, the user says: "通过 merge 之后得到的 IV，计算其等价的 XOR 值，其低位等于 0"
            // This applies to the IV converted back to full XOR.
            // For merge_inplace result (Item1), it is already the "suffix".
            // So we don't check zeros on Item1 directly unless we map it back to full hash.
        }
    }
}

template<typename IVType, size_t Layer>
void verify_iv_xor_zero(const LayerVec<IVType>& layer, int seed, size_t zero_bytes) {
    for (const auto& iv : layer) {
        Item0 full_xor = compute_iv_hash_generic(seed, iv);
        for (size_t i = 0; i < zero_bytes; ++i) {
            if (full_xor.XOR[i] != 0) {
                std::cerr << "Error: IV at layer " << Layer << " has non-zero byte at " << i << std::endl;
                return;
            }
        }
    }
}

template<typename ItemType>
bool compare_layers(const std::vector<ItemType>& list1, const std::vector<ItemType>& list2, const char* label) {
    if (list1.size() != list2.size()) {
        std::cerr << label << ": Size mismatch! " << list1.size() << " vs " << list2.size() << std::endl;
        return false;
    }
    
    // Sort both lists
    std::vector<ItemType> sorted1 = list1;
    std::vector<ItemType> sorted2 = list2;
    std::sort(sorted1.begin(), sorted1.end(), ItemComparator<ItemType>());
    std::sort(sorted2.begin(), sorted2.end(), ItemComparator<ItemType>());
    
    for (size_t i = 0; i < sorted1.size(); ++i) {
        if (std::memcmp(sorted1[i].XOR, sorted2[i].XOR, sizeof(sorted1[i].XOR)) != 0) {
            std::cerr << label << ": Mismatch at index " << i << std::endl;
            std::cerr << "List1: ";
            for(size_t k=0; k<sizeof(sorted1[i].XOR); ++k) printf("%02x", sorted1[i].XOR[k]);
            std::cerr << "\nList2: ";
            for(size_t k=0; k<sizeof(sorted2[i].XOR); ++k) printf("%02x", sorted2[i].XOR[k]);
            std::cerr << std::endl;
            return false;
        }
    }
    std::cout << label << ": MATCH (" << list1.size() << " items)" << std::endl;
    return true;
}

int main() {
    int seed = 12345;
    std::cout << "Initializing Layer 0..." << std::endl;
    
    // 1. Initialize Layer 0
    // Use init_layer_with_new_memory to allocate backing storage
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
    std::cout << "\n--- Testing Layer 0 -> Layer 1 ---" << std::endl;
    
    // Baseline: merge_inplace
    size_t layer1_bytes = MAX_LIST_SIZE * sizeof(Item1);
    Layer1 layer1 = init_layer_with_new_memory<Item1>(layer1_bytes);
    // layer1.reserve(MAX_LIST_SIZE); // Already reserved by init_layer
    
    long long t_base = measure_ms([&]() {
        merge0_inplace(layer0, layer1);
    });
    std::cout << "merge_inplace (Baseline): " << t_base << " ms, size=" << layer1.size() << std::endl;
    
    // Pure IV
    size_t layer1_iv_bytes = MAX_LIST_SIZE * sizeof(IV1);
    Layer1_IV layer1_iv = init_layer_with_new_memory<IV1>(layer1_iv_bytes);
    
    long long t_iv = measure_ms([&]() {
        merge0_iv_inplace(layer0_iv, layer1_iv, seed);
    });
    std::cout << "merge_iv_layer (Pure IV): " << t_iv << " ms, size=" << layer1_iv.size() << std::endl;
    
    // IV Key
    size_t layer1_ivkey_bytes = MAX_LIST_SIZE * sizeof(IVKey1_24);
    Layer1_IVKey layer1_ivkey = init_layer_with_new_memory<IVKey1_24>(layer1_ivkey_bytes);
    
    long long t_setkey0 = measure_ms([&]() {
        SetIV_Key_Batch<0, 24, uint32_t>(seed, layer0_ivkey);
    });
    long long t_ivkey = measure_ms([&]() {
        merge0_ivkey_layer(layer0_ivkey, layer1_ivkey, seed);
    });
    long long t_setkey1 = measure_ms([&]() {
        SetIV_Key_Batch<1, 24, uint32_t>(seed, layer1_ivkey);
    });
    
    std::cout << "SetIV_Key (Layer 0): " << t_setkey0 << " ms" << std::endl;
    std::cout << "merge_ivkey_layer (IV Key): " << t_ivkey << " ms, size=" << layer1_ivkey.size() << std::endl;
    std::cout << "SetIV_Key (Layer 1): " << t_setkey1 << " ms" << std::endl;
    std::cout << "Total IV Key time: " << (t_setkey0 + t_ivkey + t_setkey1) << " ms" << std::endl;
    
    // Verification Layer 1
    std::cout << "Verifying Layer 1..." << std::endl;
    verify_iv_xor_zero<IV1, 1>(layer1_iv, seed, 3);
    verify_iv_xor_zero<IVKey1_24, 1>(layer1_ivkey, seed, 3);
    
    // Convert to Items for comparison
    // Note: layer1 (from merge_inplace) is already vector<Item1>
    std::vector<Item1> items_base(layer1.begin(), layer1.end());
    std::vector<Item1> items_iv = iv_layer_to_items<IV1, Item1, 1>(layer1_iv, seed);
    std::vector<Item1> items_ivkey = iv_layer_to_items<IVKey1_24, Item1, 1>(layer1_ivkey, seed);
    
    compare_layers(items_base, items_iv, "Baseline vs Pure IV (Layer 1)");
    compare_layers(items_base, items_ivkey, "Baseline vs IV Key (Layer 1)");
    
    // -------------------------------------------------------------------------
    // Layer 1 -> Layer 2
    // -------------------------------------------------------------------------
    std::cout << "\n--- Testing Layer 1 -> Layer 2 ---" << std::endl;
    
    // Baseline: merge_inplace
    size_t layer2_bytes = MAX_LIST_SIZE * sizeof(Item2);
    Layer2 layer2 = init_layer_with_new_memory<Item2>(layer2_bytes);
    
    t_base = measure_ms([&]() {
        merge1_inplace(layer1, layer2);
    });
    std::cout << "merge_inplace (Baseline): " << t_base << " ms, size=" << layer2.size() << std::endl;
    
    // Pure IV
    size_t layer2_iv_bytes = MAX_LIST_SIZE * sizeof(IV2);
    Layer2_IV layer2_iv = init_layer_with_new_memory<IV2>(layer2_iv_bytes);
    
    t_iv = measure_ms([&]() {
        merge1_iv_layer(layer1_iv, layer2_iv, seed);
    });
    std::cout << "merge_iv_layer (Pure IV): " << t_iv << " ms, size=" << layer2_iv.size() << std::endl;
    
    // IV Key
    size_t layer2_ivkey_bytes = MAX_LIST_SIZE * sizeof(IVKey2_24);
    Layer2_IVKey layer2_ivkey = init_layer_with_new_memory<IVKey2_24>(layer2_ivkey_bytes);
    
    // Note: layer1_ivkey already has keys set from previous step (t_setkey1)
    t_ivkey = measure_ms([&]() {
        merge1_ivkey_layer(layer1_ivkey, layer2_ivkey, seed);
    });
    long long t_setkey2 = measure_ms([&]() {
        SetIV_Key_Batch<2, 24, uint32_t>(seed, layer2_ivkey);
    });
    
    std::cout << "merge_ivkey_layer (IV Key): " << t_ivkey << " ms, size=" << layer2_ivkey.size() << std::endl;
    std::cout << "SetIV_Key (Layer 2): " << t_setkey2 << " ms" << std::endl;
    std::cout << "Total IV Key time (merge + setkey2): " << (t_ivkey + t_setkey2) << " ms" << std::endl;
    
    // Verification Layer 2
    std::cout << "Verifying Layer 2..." << std::endl;
    verify_iv_xor_zero<IV2, 2>(layer2_iv, seed, 6);
    verify_iv_xor_zero<IVKey2_24, 2>(layer2_ivkey, seed, 6);
    
    std::vector<Item2> items2_base(layer2.begin(), layer2.end());
    std::vector<Item2> items2_iv = iv_layer_to_items<IV2, Item2, 2>(layer2_iv, seed);
    std::vector<Item2> items2_ivkey = iv_layer_to_items<IVKey2_24, Item2, 2>(layer2_ivkey, seed);
    
    compare_layers(items2_base, items2_iv, "Baseline vs Pure IV (Layer 2)");
    compare_layers(items2_base, items2_ivkey, "Baseline vs IV Key (Layer 2)");
    
    return 0;
}
