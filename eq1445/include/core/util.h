#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <vector>
#include <random>
#include <unordered_map>
#include <iostream>
#include "core/merge.h"
#include "core/zcash_blake.h"

typedef std::vector<size_t> Solution;

template <typename ItemT>
inline void print_item_hash(const char *label, const ItemT &item,
                            size_t idx = 0)
{
    if (!g_verbose)
        return;
    ::printf("%s[%zu]: ", label, idx);
    for (size_t i = 0; i < sizeof(item.XOR); ++i)
    {
        ::printf("%02x", item.XOR[i]);
    }
    ::printf("\n");
}

template <typename LayerT>
inline void print_layer_debug(const char *name, const LayerT &layer,
                              size_t show_count = 3)
{
    if (!g_verbose)
        return;
    ::printf("[DEBUG] %s: size=%zu\n", name, layer.size());
    size_t n = std::min(show_count, layer.size());
    for (size_t i = 0; i < n; ++i)
    {
        ::printf("  [%zu]: ", i);
        for (size_t j = 0; j < sizeof(layer[i].XOR); ++j)
        {
            ::printf("%02x", layer[i].XOR[j]);
        }
        if constexpr (sizeof(layer[i]) > sizeof(layer[i].XOR))
        {
            ::printf(" idx=%zu", get_index_from_bytes(layer[i].index));
        }
        ::printf("\n");
    }
}

inline void print_solution_indices(const char *label,
                                   const std::vector<size_t> &indices)
{
    if (!g_verbose)
        return;
    ::printf("%s (%zu indices): ", label, indices.size());
    size_t show_n = std::min<size_t>(16, indices.size());
    for (size_t i = 0; i < show_n; ++i)
    {
        ::printf("%zx ", indices[i]);
    }
    if (indices.size() > show_n)
        ::printf("...");
    ::printf("\n");
}

// ---------- L0 filling ----------
#ifdef __cplusplus
extern "C" {
#endif
#include "blake2bip.h"  // For 4-way Blake2b
#ifdef __cplusplus
}
#endif

template <typename Layer_Type>
inline void fill_layer0(Layer_Type &L0, int seed)
{
    // Match Tromp's implementation exactly:
    // headernonce is 140 bytes with nonce at position 108 (word 27)
    uint8_t headernonce[140];
    std::memset(headernonce, 0, sizeof(headernonce));

    // Put nonce at position 108 (as u32 word 27) in little-endian format
    // This matches: ((u32 *)headernonce)[27] = htole32(nonce);
    uint32_t *nonce_ptr = (uint32_t *)(headernonce + 108);
    *nonce_ptr = seed; // Already little-endian on x86

    // allocate L0
    using ValueType = typename Layer_Type::value_type;
    static constexpr uint32_t HALF = EquihashParams::kLeafCountHalf;
    static constexpr uint32_t FULL = EquihashParams::kLeafCountFull;
    static constexpr size_t XOR_SLICE = ItemXorSize<ValueType>;
    L0.resize(FULL);

    // Build midstate with headernonce (no separate nonce array)
    // We need to match Tromp's setheader which only takes the 140-byte
    // headernonce
    uint8_t dummy_nonce[32];
    std::memset(dummy_nonce, 0, sizeof(dummy_nonce));

    ZcashEquihashHasher H;
    H.init_midstate(headernonce, sizeof(headernonce), dummy_nonce,
                    EquihashParams::N,
                    EquihashParams::K);

    // Process indices in blocks of 4 (NBLAKES=4)
    uint8_t out[4][ZcashEquihashHasher::OUT_LEN];
    uint32_t i = 0;
    for (; i + 3 < HALF; i += 4)
    {
#ifdef NBLAKES
        // 4-way Blake2b: output 4 x 64 bytes, take first 50 bytes from each
        uint8_t hashes[4 * 64];
        blake2bx4_final(&H.mid_, hashes, i / 4);  // i/4 is block index
        for (int lane = 0; lane < 4; ++lane)
        {
            const uint8_t *ph = hashes + lane * 64;
            std::memcpy(out[lane], ph, ZcashEquihashHasher::OUT_LEN);  // Copy first 50 bytes
        }
#else
        H.hash_index(i + 0, out[0]);
        H.hash_index(i + 1, out[1]);
        H.hash_index(i + 2, out[2]);
        H.hash_index(i + 3, out[3]);
#endif

        // Store 8 leaves (2 per index)
        for (int lane = 0; lane < 4; ++lane)
        {
            const int base = (2 * (i + lane));
            std::memcpy(L0[base + 0].XOR, out[lane] + 0, XOR_SLICE);
            std::memcpy(L0[base + 1].XOR, out[lane] + XOR_SLICE, XOR_SLICE);
        }
    }
    // tail
    for (; i < HALF; ++i)
    {
        H.hash_index(i, out[0]);
        std::memcpy(L0[2 * i + 0].XOR, out[0] + 0, XOR_SLICE);
        std::memcpy(L0[2 * i + 1].XOR, out[0] + XOR_SLICE, XOR_SLICE);
    }
}

inline Item0 compute_ith_item(int seed, size_t leaf_index)
{
    // compute the same leaf value that fill_layer0 writes at L0[leaf_index]
    // leaf_index is in [0 .. FULL-1]; each pair of leaves corresponds to a
    // single hash output: L0[2*i+0] = out[0..24], L0[2*i+1] = out[25..49]
    uint8_t headernonce[140];
    std::memset(headernonce, 0, sizeof(headernonce));

    uint32_t *nonce_ptr = (uint32_t *)(headernonce + 108);
    *nonce_ptr = seed; // Already little-endian on x86

    uint8_t dummy_nonce[32];
    std::memset(dummy_nonce, 0, sizeof(dummy_nonce));

    ZcashEquihashHasher H;
    H.init_midstate(headernonce, sizeof(headernonce), dummy_nonce,
                    EquihashParams::N,
                    EquihashParams::K);

    const uint32_t pair_idx = static_cast<uint32_t>(leaf_index / 2);
    const bool second = (leaf_index & 1) != 0;

    uint8_t out[ZcashEquihashHasher::OUT_LEN];
    H.hash_index(pair_idx, out);

    Item0 item;
    constexpr size_t XOR_SLICE = ItemXorSize<Item0>;
    const size_t offset = second ? XOR_SLICE : 0;
    std::memcpy(item.XOR, out + offset, XOR_SLICE);
    return item;
}

template <typename Layer_Type>
inline void fill_layer_from_mt(Layer_Type &L0, int seed)
{
    using ValueType = typename Layer_Type::value_type;
    static constexpr size_t xor_len = ItemXorSize<ValueType>;
    static constexpr uint32_t HALF = EquihashParams::kLeafCountHalf;
    static constexpr uint32_t FULL = EquihashParams::kLeafCountFull;

    L0.resize(FULL);

    std::mt19937_64 rng(static_cast<uint64_t>(seed));
    auto fill_bytes = [&](uint8_t *buf, size_t len)
    {
        size_t pos = 0;
        while (pos < len)
        {
            uint64_t v = rng();
            size_t copy = std::min<size_t>(sizeof(v), len - pos);
            std::memcpy(buf + pos, &v, copy);
            pos += copy;
        }
    };

    uint8_t out[xor_len * 2];

    for (uint32_t i = 0; i < HALF; ++i)
    {
        fill_bytes(out, sizeof(out));
        std::memcpy(L0[2 * i + 0].XOR, out + 0, xor_len);
        std::memcpy(L0[2 * i + 1].XOR, out + xor_len, xor_len);
    }
}

inline void expand_solution(Solution &solution, const Layer_IP &IP)
{
    if (IP.empty())
    {
        solution.clear();
        return;
    }
    Solution out;
    out.reserve(solution.size() * 2);
    for (size_t idx_ref : solution)
    {
        if (idx_ref >= IP.size()){
            assert(false && "Index out of bounds in expand_solution");
        }
        const Item_IP &ip = IP[idx_ref];
        out.push_back(get_index_from_bytes(ip.index_pointer_left));
        out.push_back(get_index_from_bytes(ip.index_pointer_right));
    }
    solution.swap(out);
}

inline void expand_solution_from_file(Solution &solution,
                                      IPDiskReader<Item_IP> &reader,
                                      const IPDiskMeta &meta)
{
    Solution out;
    out.reserve(solution.size() * 2);
    for (size_t idx_ref : solution)
    {
        if (idx_ref >= meta.count)
        {
            std::cout << "Error: idx_ref " << idx_ref << " >= meta.count " << meta.count << std::endl;
            assert(false && "Index out of bounds in expand_solution_from_file");
        }
        Item_IP ip{};
        bool succ  = reader.read_ip_item(meta.offset + idx_ref * sizeof(Item_IP), ip);
        if (!succ)
        {
            std::cerr << "Error reading IP item from file at index " << idx_ref << std::endl;
            assert(false && "Failed to read IP item from file");
        }
        out.push_back(get_index_from_bytes(ip.index_pointer_left));
        out.push_back(get_index_from_bytes(ip.index_pointer_right));
    }
    solution.swap(out);
}

inline void expand_solutions_from_file(std::vector<Solution> &solutions,
                                       IPDiskReader<Item_IP> &reader,
                                       const IPDiskMeta &meta)
{
    if (meta.count == 0 || solutions.empty())
        return;
    
    for (auto &sol : solutions)
    {
        expand_solution_from_file(sol, reader, meta);
    }
}

inline void expand_solutions(std::vector<Solution> &solutions, const Layer_IP &IP)
{
    if (IP.empty())
        return;
    if (solutions.empty())
    {
        solutions.resize(IP.size());
        for (size_t i = 0; i < IP.size(); ++i)
        {
            solutions[i].reserve(2);
            solutions[i].push_back(get_index_from_bytes(IP[i].index_pointer_left));
            solutions[i].push_back(get_index_from_bytes(IP[i].index_pointer_right));
        }
        return;
    }
    else
    {
        for (auto &sol : solutions)
        {
            expand_solution(sol, IP);
        }
    }
}

static inline bool is_trivial_solution(const Solution &solution)
{
    std::unordered_map<size_t, size_t> cnt;
    cnt.reserve(solution.size() * 2);
    for (size_t x : solution)
        cnt[x]++;
    for (auto &kv : cnt)
        if ((kv.second & 1) != 0)
            return false;
    return true;
}

/**
 * @brief Remove trivial solutions from expanded in-place.
 *
 * A trivial solution is one where every index appears an even number of times.
 * After calling this, `expanded` will only contain non-trivial chains.
 */
inline void filter_trivial_solutions(std::vector<Solution> &solutions)
{
    auto it = std::remove_if(solutions.begin(), solutions.end(),
                             [](const Solution &solution)
                             {
                                 return is_trivial_solution(solution);
                             });
    solutions.erase(it, solutions.end());
}

static inline size_t check_zero_xor(
    const int &seed, const std::vector<Solution> &solutions)
{
    constexpr size_t xor_len = ItemXorSize<Item0>;
    auto is_zero = [](const std::array<uint8_t, xor_len> &acc)
    {
        for (uint8_t b : acc)
        {
            if (b)
                return false;
        }
        return true;
    };

    const size_t total = solutions.size();
    size_t trivial = 0, valid = 0;

    for (size_t ci = 0; ci < total; ++ci)
    {
        const auto &chain = solutions[ci];
        if (is_trivial_solution(chain))
        {
            ++trivial;
            continue;
        }

        std::array<uint8_t, xor_len> acc{};
        acc.fill(0);
        for (size_t idx : chain)
        {
            Item0 item = compute_ith_item(seed, idx);
            for (size_t j = 0; j < xor_len; ++j)
                acc[j] ^= item.XOR[j];
        }

        bool is_zero_chain = is_zero(acc);
        if (is_zero_chain)
            ++valid;

        if (g_verbose)
        {
            ::printf("Chain %zu (size=%zu): XOR result = ", ci, chain.size());
            for (size_t j = 0; j < xor_len; ++j)
                ::printf("%02x", acc[j]);
            if (is_zero_chain)
            {
                ::printf(" ✓ VALID zero-XOR solution\n");
                ::printf("    Indices: ");
                size_t show_n = std::min<size_t>(16, chain.size());
                for (size_t i = 0; i < show_n; ++i)
                    ::printf("%zx ", chain[i]);
                if (chain.size() > show_n)
                    ::printf("... (total %zu)", chain.size());
                ::printf("\n");
            }
        }
    }

    if (g_verbose)
    {
        std::cout << "---------------------------------------------------------"
                     "----------------------\n";
        std::cout << "Solution Statistics:\n";
        std::cout << "  Total chains: " << total << "\n";
        std::cout << "  Trivial chains: " << trivial << "\n";
        std::cout << "  Valid solutions: " << valid << "\n";
        std::cout << "========================================================="
                     "======================\n";
    }
    return valid;
}


// Measure peak RSS (kB)
static long peak_rss_kb()
{
    // Use Linux /proc/self/status
    FILE *f = fopen("/proc/self/status", "r");
    if (!f)
        return -1;
    char line[512];
    long kb = -1;
    while (fgets(line, sizeof(line), f))
    {
        if (std::strncmp(line, "VmHWM:", 6) == 0)
        {
            char *p = std::strrchr(line, '\t');
            if (!p)
                p = line;
            long v = 0;
            if (sscanf(line + 6, "%ld", &v) == 1)
            {
                kb = v;
                break;
            }
        }
    }
    fclose(f);
    return kb;
}

// Measure current USS (Unique Set Size) in kilobytes.
// This reads /proc/self/smaps_rollup and sums Private_Dirty and Private_Clean
// which approximates the process' private RSS (USS). If smaps_rollup is not
// available, fall back to VmRSS from /proc/self/status (less precise).
static inline long current_uss_kb()
{
    // Prefer smaps_rollup when available (aggregated per-process values).
    FILE *f = fopen("/proc/self/smaps_rollup", "r");
    if (f)
    {
        char line[256];
        long private_dirty = 0;
        long private_clean = 0;
        while (fgets(line, sizeof(line), f))
        {
            if (std::strncmp(line, "Private_Dirty:", 14) == 0)
            {
                long v = 0;
                if (sscanf(line + 14, "%ld", &v) == 1)
                    private_dirty += v;
            }
            else if (std::strncmp(line, "Private_Clean:", 14) == 0)
            {
                long v = 0;
                if (sscanf(line + 14, "%ld", &v) == 1)
                    private_clean += v;
            }
        }
        fclose(f);
        return private_dirty + private_clean;
    }
    // cannot read, error
    std::cerr << "Warning: cannot open /proc/self/smaps_rollup for USS measurement, falling back to VmRSS\n";
    return -1;
}

static inline void debug_print_uss(const char *tag)
{
    long kb = current_uss_kb();
    std::cout << "[USS] " << tag << " USS=" << kb << " kB\n";
}

static inline void debug_print_rss(const char *tag)
{
    size_t kb = peak_rss_kb();
    std::cout << "[RSS] " << tag << " VmRSS=" << kb << " kB\n";
}