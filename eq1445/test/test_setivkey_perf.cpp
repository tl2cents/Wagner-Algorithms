#include "eq144_5/equihash_144_5.h"
#include "eq144_5/sort_144_5.h"
#include "core/util.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>

using namespace std::chrono;

// Global variables required by sort.h
SortAlgo g_sort_algo = SortAlgo::KXSORT;
bool g_verbose = false;

template<typename Func>
long long measure_ms(Func f) {
    auto start = high_resolution_clock::now();
    f();
    auto end = high_resolution_clock::now();
    return duration_cast<milliseconds>(end - start).count();
}

int main() {
    int seed = 12345;
    std::cout << "Performance Test: fill_layer0 vs SetIV_Key_Batch" << std::endl;
    std::cout << "Leaf Count: " << EquihashParams::kLeafCountFull << std::endl;

    // 1. Measure fill_layer0
    size_t layer0_bytes = EquihashParams::kLeafCountFull * sizeof(Item0);
    Layer0 layer0 = init_layer_with_new_memory<Item0>(layer0_bytes);
    
    long long t_fill = measure_ms([&]() {
        fill_layer0(layer0, seed);
    });
    std::cout << "fill_layer0 time: " << t_fill << " ms" << std::endl;

    // 2. Measure SetIV_Key_Batch (Layer 0)
    // We need an IVKey layer.
    size_t layer0_ivkey_bytes = EquihashParams::kLeafCountFull * sizeof(IVKey0_24);
    Layer0_IVKey layer0_ivkey = init_layer_with_new_memory<IVKey0_24>(layer0_ivkey_bytes);
    layer0_ivkey.resize(EquihashParams::kLeafCountFull);
    
    // Initialize indices
    for (size_t i = 0; i < layer0_ivkey.size(); ++i) {
        layer0_ivkey[i].set_index(0, i);
    }

    // Debug single item
    SetIV_Key<0, 24, uint32_t>(seed, layer0_ivkey[0]);
    std::cout << "Debug: layer0_ivkey[0].key = " << layer0_ivkey[0].key << std::endl;
    Item0 item0 = compute_ith_item(seed, 0);
    std::cout << "Debug: compute_ith_item(0) XOR[0] = " << (int)item0.XOR[0] << std::endl;

    long long t_setkey0 = measure_ms([&]() {
        SetIV_Key_Batch<0, 24, uint32_t>(seed, layer0_ivkey);
    });
    uint64_t check0 = 0;
    for(size_t i=0; i<100; ++i) check0 += layer0_ivkey[i].key;
    std::cout << "SetIV_Key_Batch (Layer 0) time: " << t_setkey0 << " ms (check: " << check0 << ")" << std::endl;
    
    if (t_fill > 0) {
        double ratio = (double)t_setkey0 / t_fill;
        std::cout << "Ratio (SetIV_Key / fill_layer0): " << std::fixed << std::setprecision(2) << ratio << "x" << std::endl;
    }

    // Verify Layer 0 consistency
    std::cout << "Verifying Layer 0 keys..." << std::endl;
    size_t errors = 0;
    if (layer0.size() != layer0_ivkey.size()) {
        std::cout << "Size mismatch: layer0=" << layer0.size() << " layer0_ivkey=" << layer0_ivkey.size() << std::endl;
        errors++;
    } else {
        for (size_t i = 0; i < layer0.size(); ++i) {
            uint32_t key_iv = layer0_ivkey[i].key;
            uint32_t key_ref = 0;
            // Copy 3 bytes (24 bits) from XOR to key_ref
            std::memcpy(&key_ref, layer0[i].XOR, 3);
            
            if (key_iv != key_ref) {
                std::cout << "Mismatch at index " << i << ": IVKey=" << key_iv << " Ref=" << key_ref << std::endl;
                errors++;
                if (errors > 10) break;
            }
        }
    }

    if (errors == 0) {
        std::cout << "Verification PASSED: All keys match." << std::endl;
    } else {
        std::cout << "Verification FAILED: " << errors << " mismatches found." << std::endl;
    }

    // 3. Measure SetIV_Key_Batch (Layer 1)
    // Use a smaller size for Layer 1 and 2 to save time, but scale result for comparison if needed.
    // Or use full size to be sure. 33M items is fine.
    size_t test_size = EquihashParams::kLeafCountFull; 
    
    size_t layer1_ivkey_bytes = test_size * sizeof(IVKey1_24);
    Layer1_IVKey layer1_ivkey = init_layer_with_new_memory<IVKey1_24>(layer1_ivkey_bytes);
    layer1_ivkey.resize(test_size);
    // Initialize with dummy indices
    for (size_t i = 0; i < test_size; ++i) {
        layer1_ivkey[i].set_index(0, i*2);
        layer1_ivkey[i].set_index(1, i*2+1);
    }

    long long t_setkey1 = measure_ms([&]() {
        SetIV_Key_Batch<1, 24, uint32_t>(seed, layer1_ivkey);
    });
    uint64_t check1 = 0;
    for(size_t i=0; i<100; ++i) check1 += layer1_ivkey[i].key;
    std::cout << "SetIV_Key_Batch (Layer 1, " << test_size << " items) time: " << t_setkey1 << " ms (check: " << check1 << ")" << std::endl;

    // 4. Measure SetIV_Key_Batch (Layer 2)
    size_t layer2_ivkey_bytes = test_size * sizeof(IVKey2_24);
    Layer2_IVKey layer2_ivkey = init_layer_with_new_memory<IVKey2_24>(layer2_ivkey_bytes);
    layer2_ivkey.resize(test_size);
    for (size_t i = 0; i < test_size; ++i) {
        for(int k=0; k<4; ++k) layer2_ivkey[i].set_index(k, i*4+k);
    }

    long long t_setkey2 = measure_ms([&]() {
        SetIV_Key_Batch<2, 24, uint32_t>(seed, layer2_ivkey);
    });
    uint64_t check2 = 0;
    for(size_t i=0; i<100; ++i) check2 += layer2_ivkey[i].key;
    std::cout << "SetIV_Key_Batch (Layer 2, " << test_size << " items) time: " << t_setkey2 << " ms (check: " << check2 << ")" << std::endl;

    return 0;
}
