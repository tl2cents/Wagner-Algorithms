#include "eq144_5/equihash_144_5.h"
#include "eq144_5/sort_144_5.h"
#include "eq144_5/merge_144_5.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

// Global variables required by sort.h
SortAlgo g_sort_algo = SortAlgo::KXSORT;
bool g_verbose = false;

namespace
{
std::string to_hex(const uint8_t *data, size_t len)
{
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i)
    {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(data[i]);
    }
    return oss.str();
}

template <size_t Layer, size_t KeyBits>
auto reference_key_bits(const Item0 &xiv)
{
    constexpr size_t OFFSET_BITS = EquihashParams::kCollisionBitLength * Layer;
    using KeyType = std::conditional_t<(KeyBits <= 32), uint32_t, uint64_t>;
    if constexpr (KeyBits == 0)
    {
        return KeyType{0};
    }

    KeyType key = 0;
    const uint8_t *bytes = xiv.XOR;
    for (size_t bit = 0; bit < KeyBits; ++bit)
    {
        const size_t absolute_bit = OFFSET_BITS + bit;
        const size_t byte_index = absolute_bit / 8;
        const size_t bit_index = absolute_bit % 8;
        const KeyType bit_val = static_cast<KeyType>((bytes[byte_index] >> bit_index) & 0x1u);
        key |= (bit_val << bit);
    }
    return key;
}

template <size_t Layer>
void verify_iv_keys(const IV<Layer> &iv, int seed, const char *label)
{
    constexpr size_t kCount = IV<Layer>::NumIndices;
    std::cout << "--- " << label << " (Layer " << Layer << ", "
              << kCount << " indices) ---\n";

    Item0 manual{};
    std::memset(manual.XOR, 0, sizeof(manual.XOR));
    for (size_t i = 0; i < kCount; ++i)
    {
        const size_t idx = iv.get_index(i);
        const Item0 leaf = compute_ith_item(seed, idx);
        std::cout << "  leaf[" << i << "] idx=" << idx
                  << " hex=" << to_hex(leaf.XOR, sizeof(leaf.XOR)) << '\n';
        for (size_t b = 0; b < sizeof(manual.XOR); ++b)
        {
            manual.XOR[b] ^= leaf.XOR[b];
        }
    }

    const Item0 xiv = compute_iv_hash<Layer>(seed, iv);
    const std::string manual_hex = to_hex(manual.XOR, sizeof(manual.XOR));
    const std::string xiv_hex = to_hex(xiv.XOR, sizeof(xiv.XOR));
    std::cout << "  Manual XOR:      " << manual_hex << '\n';
    std::cout << "  compute_iv_hash: " << xiv_hex << '\n';
    if (manual_hex != xiv_hex)
    {
        std::cerr << "Mismatch between manual XOR and compute_iv_hash!" << std::endl;
        std::exit(1);
    }

    constexpr size_t offset_bits = EquihashParams::kCollisionBitLength * Layer;
    std::cout << "  Offset bits: " << offset_bits
              << " (byte offset " << offset_bits / 8 << ")\n";

    const auto key24 = getKey24<Layer>(seed, iv);
    const auto key48 = getKey48<Layer>(seed, iv);
    const auto ref24 = reference_key_bits<Layer, EQ1445_COLLISION_BITS>(xiv);
    const auto ref48 = reference_key_bits<Layer, EQ1445_FINAL_BITS>(xiv);

    std::cout << std::hex;
    std::cout << "  getKey24: 0x" << key24 << " (ref 0x" << ref24 << ")\n";
    std::cout << "  getKey48: 0x" << key48 << " (ref 0x" << ref48 << ")\n";
    std::cout << std::dec;
    if (key24 != ref24 || key48 != ref48)
    {
        std::cerr << "getKey mismatch detected!" << std::endl;
        std::exit(1);
    }

    std::cout << std::endl;
}

// =============================================================================
// Test: IV0 sorting vs Item0 sorting consistency
// =============================================================================
void test_iv0_sort_consistency(int seed)
{
    // Use a smaller subset for testing (full 2^25 would be slow)
    constexpr size_t TEST_SIZE = 10000;
    std::cout << "Testing with " << TEST_SIZE << " elements (subset of full layer)...\n";

    // 1. Create Layer0 with Item0 and fill + sort by 24-bit key
    std::vector<Item0> layer0_items(TEST_SIZE);
    {
        // Fill layer0 items using compute_ith_item
        for (size_t i = 0; i < TEST_SIZE; ++i)
        {
            layer0_items[i] = compute_ith_item(seed, i);
        }
        // Sort by 24-bit key (collision bits)
        std::sort(layer0_items.begin(), layer0_items.end(),
                  [](const Item0 &a, const Item0 &b)
                  {
                      return getKey24(a) < getKey24(b);
                  });
    }

    // 2. Create Layer0_IV with IV0 (each IV0 stores 1 index)
    std::vector<IV0> layer0_iv(TEST_SIZE);
    {
        for (size_t i = 0; i < TEST_SIZE; ++i)
        {
            layer0_iv[i].set_index(0, i);
        }
        // Sort by 24-bit key using getKey24 for IV
        std::sort(layer0_iv.begin(), layer0_iv.end(),
                  [seed](const IV0 &a, const IV0 &b)
                  {
                      return getKey24<0>(seed, a) < getKey24<0>(seed, b);
                  });
    }

    // 3. Verify: after sorting, the keys should be in the same order
    std::cout << "Verifying sorted order consistency...\n";
    bool all_match = true;
    size_t mismatch_count = 0;
    for (size_t i = 0; i < TEST_SIZE; ++i)
    {
        const uint32_t key_item = getKey24(layer0_items[i]);
        const uint32_t key_iv = getKey24<0>(seed, layer0_iv[i]);

        if (key_item != key_iv)
        {
            if (mismatch_count < 5)
            {
                std::cerr << "  Mismatch at position " << i << ": "
                          << "Item0 key=0x" << std::hex << key_item
                          << ", IV0 key=0x" << key_iv << std::dec << "\n";
            }
            all_match = false;
            ++mismatch_count;
        }
    }

    if (all_match)
    {
        std::cout << "  ✓ All " << TEST_SIZE << " keys match after sorting!\n";
    }
    else
    {
        std::cerr << "  ✗ Total mismatches: " << mismatch_count << "\n";
        std::exit(1);
    }

    // 4. Additional check: verify the underlying indices point to correct leaves
    std::cout << "Verifying IV indices map to correct hashes...\n";
    for (size_t i = 0; i < std::min<size_t>(10, TEST_SIZE); ++i)
    {
        const size_t idx = layer0_iv[i].get_index(0);
        const Item0 leaf = compute_ith_item(seed, idx);
        const Item0 &sorted_item = layer0_items[i];

        // Keys must match
        if (getKey24(leaf) != getKey24(sorted_item))
        {
            std::cerr << "  Index-to-hash mismatch at sorted position " << i << "\n";
            std::exit(1);
        }
    }
    std::cout << "  ✓ IV indices correctly map to leaf hashes!\n";

    // 5. Show some sample sorted entries
    std::cout << "\nSample sorted entries (first 5):\n";
    std::cout << std::hex;
    for (size_t i = 0; i < std::min<size_t>(5, TEST_SIZE); ++i)
    {
        const size_t idx = layer0_iv[i].get_index(0);
        std::cout << "  [" << i << "] IV0 idx=" << std::dec << idx
                  << " key=0x" << std::hex << getKey24<0>(seed, layer0_iv[i])
                  << " Item0 key=0x" << getKey24(layer0_items[i]) << "\n";
    }
    std::cout << std::dec << "\n";
}

// =============================================================================
// Test: Compare IV vs IV_Key merge performance
// =============================================================================
void test_ivkey_vs_iv_performance(int seed)
{
    constexpr size_t TEST_SIZE = 50000;
    std::cout << "=== Performance Comparison: IV vs IV_Key (Layer 1 -> Layer 2) ===" << std::endl;
    std::cout << "Testing with " << TEST_SIZE << " Layer 1 items...\n\n";

    // Allocate memory for both approaches
    const size_t mem_size = TEST_SIZE * 2 * sizeof(IV2);
    uint8_t *mem_iv = static_cast<uint8_t *>(std::malloc(mem_size));
    uint8_t *mem_ivkey = static_cast<uint8_t *>(std::malloc(mem_size));
    
    if (!mem_iv || !mem_ivkey)
    {
        std::cerr << "Failed to allocate memory!" << std::endl;
        std::exit(1);
    }

    // =============================================================================
    // Test 1: Pure IV approach (recompute hash during sort)
    // =============================================================================
    std::cout << "--- Approach 1: Pure IV (hash recomputation) ---\n";
    
    Layer1_IV src_iv = init_layer<IV1>(mem_iv, mem_size);
    Layer2_IV dst_iv = init_layer<IV2>(mem_iv, mem_size);
    
    // Fill Layer 1 IV with test data
    src_iv.resize(TEST_SIZE);
    for (size_t i = 0; i < TEST_SIZE; ++i)
    {
        src_iv[i].set_index(0, i * 2);
        src_iv[i].set_index(1, i * 2 + 1);
    }
    
    auto t1_start = std::chrono::high_resolution_clock::now();
    
    // Merge Layer 1 -> Layer 2 (with hash recomputation during sort)
    merge1_iv_layer(src_iv, dst_iv, seed);
    
    auto t1_end = std::chrono::high_resolution_clock::now();
    auto t1_duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1_end - t1_start);
    
    std::cout << "  Input size: " << TEST_SIZE << "\n";
    std::cout << "  Output size: " << dst_iv.size() << "\n";
    std::cout << "  Time: " << t1_duration.count() << " ms\n\n";

    // =============================================================================
    // Test 2: IV_Key approach (precomputed keys)
    // =============================================================================
    std::cout << "--- Approach 2: IV_Key (precomputed keys) ---\n";
    
    Layer1_IVKey src_ivkey = init_layer<IVKey1_24>(mem_ivkey, mem_size);
    Layer2_IVKey dst_ivkey = init_layer<IVKey2_24>(mem_ivkey, mem_size);
    
    // Fill Layer 1 IV_Key with same test data
    src_ivkey.resize(TEST_SIZE);
    for (size_t i = 0; i < TEST_SIZE; ++i)
    {
        src_ivkey[i].set_index(0, i * 2);
        src_ivkey[i].set_index(1, i * 2 + 1);
    }
    
    auto t2_start = std::chrono::high_resolution_clock::now();
    
    // Step 1: Compute keys for source layer
    SetIV_Key_Batch<1, EQ1445_COLLISION_BITS, uint32_t>(seed, src_ivkey);
    
    auto t2_after_setkey = std::chrono::high_resolution_clock::now();
    
    // Step 2: Merge Layer 1 -> Layer 2 (fast sort using precomputed keys)
    merge1_ivkey_layer(src_ivkey, dst_ivkey, seed);
    
    auto t2_after_merge = std::chrono::high_resolution_clock::now();
    
    // Step 3: Compute keys for destination layer
    SetIV_Key_Batch<2, EQ1445_COLLISION_BITS, uint32_t>(seed, dst_ivkey);
    
    auto t2_end = std::chrono::high_resolution_clock::now();
    
    auto t2_setkey1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2_after_setkey - t2_start);
    auto t2_merge = std::chrono::duration_cast<std::chrono::milliseconds>(t2_after_merge - t2_after_setkey);
    auto t2_setkey2 = std::chrono::duration_cast<std::chrono::milliseconds>(t2_end - t2_after_merge);
    auto t2_total = std::chrono::duration_cast<std::chrono::milliseconds>(t2_end - t2_start);
    
    std::cout << "  Input size: " << TEST_SIZE << "\n";
    std::cout << "  Output size: " << dst_ivkey.size() << "\n";
    std::cout << "  Time breakdown:\n";
    std::cout << "    SetIV_Key (source): " << t2_setkey1.count() << " ms\n";
    std::cout << "    Merge: " << t2_merge.count() << " ms\n";
    std::cout << "    SetIV_Key (dest): " << t2_setkey2.count() << " ms\n";
    std::cout << "  Total time: " << t2_total.count() << " ms\n\n";

    // =============================================================================
    // Verify correctness: output sizes should match
    // =============================================================================
    std::cout << "--- Verification ---\n";
    if (dst_iv.size() == dst_ivkey.size())
    {
        std::cout << "  ✓ Output sizes match: " << dst_iv.size() << "\n";
    }
    else
    {
        std::cerr << "  ✗ Output size mismatch: IV=" << dst_iv.size() 
                  << " IV_Key=" << dst_ivkey.size() << "\n";
        std::exit(1);
    }
    
    // Sample verification: check that keys match for first few items
    bool keys_match = true;
    for (size_t i = 0; i < std::min<size_t>(10, dst_iv.size()); ++i)
    {
        const uint32_t key_iv = getKey24<2>(seed, dst_iv[i]);
        const uint32_t key_ivkey = dst_ivkey[i].key;
        if (key_iv != key_ivkey)
        {
            std::cerr << "  ✗ Key mismatch at index " << i << ": "
                      << "IV=0x" << std::hex << key_iv 
                      << " IV_Key=0x" << key_ivkey << std::dec << "\n";
            keys_match = false;
            break;
        }
    }
    if (keys_match)
    {
        std::cout << "  ✓ Sample keys match (first 10 items)\n";
    }

    // =============================================================================
    // Performance comparison
    // =============================================================================
    std::cout << "\n--- Performance Summary ---\n";
    std::cout << "  IV approach:     " << t1_duration.count() << " ms\n";
    std::cout << "  IV_Key approach: " << t2_total.count() << " ms\n";
    
    if (t2_total.count() < t1_duration.count())
    {
        double speedup = static_cast<double>(t1_duration.count()) / t2_total.count();
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) 
                  << speedup << "x faster\n";
    }
    else
    {
        double slowdown = static_cast<double>(t2_total.count()) / t1_duration.count();
        std::cout << "  Slowdown: " << std::fixed << std::setprecision(2) 
                  << slowdown << "x slower\n";
    }
    
    std::cout << "\n  Note: IV_Key trades memory for speed.\n";
    std::cout << "  - SetIV_Key cost: " << (t2_setkey1.count() + t2_setkey2.count()) 
              << " ms (" << std::fixed << std::setprecision(1)
              << (100.0 * (t2_setkey1.count() + t2_setkey2.count()) / t2_total.count()) 
              << "% of total)\n";
    std::cout << "  - Merge speedup from cached keys: ";
    if (t2_merge.count() < t1_duration.count())
    {
        double merge_speedup = static_cast<double>(t1_duration.count()) / t2_merge.count();
        std::cout << merge_speedup << "x\n";
    }
    else
    {
        std::cout << "slower\n";
    }

    std::free(mem_iv);
    std::free(mem_ivkey);
    std::cout << std::endl;
}

// =============================================================================
// Test: SetIV_Key complexity across layers
// =============================================================================
void test_setivkey_complexity(int seed)
{
    constexpr size_t TEST_SIZE = 10000;
    std::cout << "=== SetIV_Key Time Complexity Across Layers ===" << std::endl;
    std::cout << "Testing with " << TEST_SIZE << " items per layer...\n\n";

    const size_t mem_size = TEST_SIZE * sizeof(IVKey5_48);
    uint8_t *mem = static_cast<uint8_t *>(std::malloc(mem_size));
    
    if (!mem)
    {
        std::cerr << "Failed to allocate memory!" << std::endl;
        std::exit(1);
    }

    // Layer 0: 1 index per IV
    {
        Layer0_IVKey layer = init_layer<IVKey0_24>(mem, mem_size);
        layer.resize(TEST_SIZE);
        for (size_t i = 0; i < TEST_SIZE; ++i)
        {
            layer[i].set_index(0, i);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        SetIV_Key_Batch<0, EQ1445_COLLISION_BITS, uint32_t>(seed, layer);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Layer 0 (1 index):  " << duration.count() << " μs"
                  << " (" << std::fixed << std::setprecision(2) 
                  << (duration.count() / 1000.0) << " ms)\n";
    }

    // Layer 1: 2 indices per IV
    {
        Layer1_IVKey layer = init_layer<IVKey1_24>(mem, mem_size);
        layer.resize(TEST_SIZE);
        for (size_t i = 0; i < TEST_SIZE; ++i)
        {
            layer[i].set_index(0, i * 2);
            layer[i].set_index(1, i * 2 + 1);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        SetIV_Key_Batch<1, EQ1445_COLLISION_BITS, uint32_t>(seed, layer);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Layer 1 (2 indices): " << duration.count() << " μs"
                  << " (" << std::fixed << std::setprecision(2) 
                  << (duration.count() / 1000.0) << " ms)\n";
    }

    // Layer 2: 4 indices per IV
    {
        Layer2_IVKey layer = init_layer<IVKey2_24>(mem, mem_size);
        layer.resize(TEST_SIZE);
        for (size_t i = 0; i < TEST_SIZE; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
                layer[i].set_index(j, i * 4 + j);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        SetIV_Key_Batch<2, EQ1445_COLLISION_BITS, uint32_t>(seed, layer);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Layer 2 (4 indices): " << duration.count() << " μs"
                  << " (" << std::fixed << std::setprecision(2) 
                  << (duration.count() / 1000.0) << " ms)\n";
    }

    // Layer 3: 8 indices per IV
    {
        Layer3_IVKey layer = init_layer<IVKey3_24>(mem, mem_size);
        layer.resize(TEST_SIZE);
        for (size_t i = 0; i < TEST_SIZE; ++i)
        {
            for (size_t j = 0; j < 8; ++j)
                layer[i].set_index(j, i * 8 + j);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        SetIV_Key_Batch<3, EQ1445_COLLISION_BITS, uint32_t>(seed, layer);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Layer 3 (8 indices): " << duration.count() << " μs"
                  << " (" << std::fixed << std::setprecision(2) 
                  << (duration.count() / 1000.0) << " ms)\n";
    }

    // Layer 4: 16 indices per IV
    {
        Layer4_IVKey layer = init_layer<IVKey4_48>(mem, mem_size);
        layer.resize(TEST_SIZE);
        for (size_t i = 0; i < TEST_SIZE; ++i)
        {
            for (size_t j = 0; j < 16; ++j)
                layer[i].set_index(j, i * 16 + j);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        SetIV_Key_Batch<4, EQ1445_FINAL_BITS, uint64_t>(seed, layer);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Layer 4 (16 indices): " << duration.count() << " μs"
                  << " (" << std::fixed << std::setprecision(2) 
                  << (duration.count() / 1000.0) << " ms)\n";
    }

    std::cout << "\nObservation: SetIV_Key time grows linearly with number of indices (2^Layer)\n";
    std::cout << "Each index requires one Blake2b hash computation.\n";

    std::free(mem);
    std::cout << std::endl;
}

} // namespace

int main()
{
    std::cout << "=== Layer_IV Types for Equihash(144,5) ===" << std::endl;
    std::cout << std::endl;

    // Check IV sizes
    std::cout << "IV sizes (IndexVector only):" << std::endl;
    std::cout << "  IV1: " << sizeof(IV1) << " bytes (2 indices)" << std::endl;
    std::cout << "  IV2: " << sizeof(IV2) << " bytes (4 indices)" << std::endl;
    std::cout << "  IV3: " << sizeof(IV3) << " bytes (8 indices)" << std::endl;
    std::cout << "  IV4: " << sizeof(IV4) << " bytes (16 indices)" << std::endl;
    std::cout << "  IV5: " << sizeof(IV5) << " bytes (32 indices)" << std::endl;
    std::cout << std::endl;

    // Test template access
    std::cout << "Template access test:" << std::endl;
    std::cout << "  IV<1>: " << sizeof(IV<1>) << " bytes" << std::endl;
    std::cout << "  IV<2>: " << sizeof(IV<2>) << " bytes" << std::endl;
    std::cout << "  IV<3>: " << sizeof(IV<3>) << " bytes" << std::endl;
    std::cout << "  IV<4>: " << sizeof(IV<4>) << " bytes" << std::endl;
    std::cout << "  IV<5>: " << sizeof(IV<5>) << " bytes" << std::endl;
    std::cout << std::endl;

    // Demonstrate IV contents
    std::cout << "Basic operations test:" << std::endl;
    IV2 sample;
    sample.set_index(0, 100);
    sample.set_index(1, 200);
    sample.set_index(2, 300);
    sample.set_index(3, 400);
    std::cout << "  IV2 (4 indices): [";
    for (size_t i = 0; i < 4; ++i)
    {
        if (i > 0)
            std::cout << ", ";
        std::cout << sample.get_index(i);
    }
    std::cout << "]\n\n";

    constexpr int seed = 7;

    IV1 iv1;
    iv1.set_index(0, 0);
    iv1.set_index(1, 1);

    IV2 iv2;
    for (size_t i = 0; i < IV2::NumIndices; ++i)
    {
        iv2.set_index(i, i * 3);
    }

    IV3 iv3;
    for (size_t i = 0; i < IV3::NumIndices; ++i)
    {
        iv3.set_index(i, 20 + i * 2);
    }

    verify_iv_keys<1>(iv1, seed, "IV1 sample");
    verify_iv_keys<2>(iv2, seed, "IV2 sample");
    verify_iv_keys<3>(iv3, seed, "IV3 sample");

    std::cout << "All IV key tests passed!\n\n";

    // =========================================================================
    // Test: IV0 sorting consistency with fill_layer0 + sort
    // =========================================================================
    std::cout << "=== IV0 Sort Consistency Test ===" << std::endl;
    test_iv0_sort_consistency(seed);

    // =========================================================================
    // Test: IV vs IV_Key performance comparison
    // =========================================================================
    test_ivkey_vs_iv_performance(seed);

    // =========================================================================
    // Test: SetIV_Key time complexity across layers
    // =========================================================================
    test_setivkey_complexity(seed);

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
