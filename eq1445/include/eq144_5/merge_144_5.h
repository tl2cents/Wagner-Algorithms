#pragma once

#define MOVE_BOUND 65537 // minimum number of items to move in one batch during in-place merge
#define MAX_TMP_SIZE 66561 // maximum pre-allocated temporary buffer size during in-place merge
#define GROUP_BOUND 1024 // maximum group size during in-place merge

#include "eq144_5/equihash_144_5.h"
#include "core/merge.h"
#include "eq144_5/sort_144_5.h"

inline constexpr int ELL_BITS_144_5 =
    static_cast<int>(EquihashParams::kCollisionBitLength);

// -----------------------------------------------------------------------------
// Item merge helpers (parameter-specialized)
// -----------------------------------------------------------------------------
inline Item1 merge_item0(const Item0 &a, const Item0 &b)
{
    return merge_item_generic<Item0, Item1>(a, b, ELL_BITS_144_5);
}
inline Item2 merge_item1(const Item1 &a, const Item1 &b)
{
    return merge_item_generic<Item1, Item2>(a, b, ELL_BITS_144_5);
}
inline Item3 merge_item2(const Item2 &a, const Item2 &b)
{
    return merge_item_generic<Item2, Item3>(a, b, ELL_BITS_144_5);
}
inline Item4 merge_item3(const Item3 &a, const Item3 &b)
{
    return merge_item_generic<Item3, Item4>(a, b, ELL_BITS_144_5);
}
inline Item5 merge_item4(const Item4 &a, const Item4 &b)
{
    return merge_item_generic<Item4, Item5>(a, b, ELL_BITS_144_5);
}

inline Item1_IDX merge_item0_IDX(const Item0_IDX &a, const Item0_IDX &b)
{
    return merge_item_generic<Item0_IDX, Item1_IDX>(a, b, ELL_BITS_144_5);
}
inline Item2_IDX merge_item1_IDX(const Item1_IDX &a, const Item1_IDX &b)
{
    return merge_item_generic<Item1_IDX, Item2_IDX>(a, b, ELL_BITS_144_5);
}
inline Item3_IDX merge_item2_IDX(const Item2_IDX &a, const Item2_IDX &b)
{
    return merge_item_generic<Item2_IDX, Item3_IDX>(a, b, ELL_BITS_144_5);
}
inline Item4_IDX merge_item3_IDX(const Item3_IDX &a, const Item3_IDX &b)
{
    return merge_item_generic<Item3_IDX, Item4_IDX>(a, b, ELL_BITS_144_5);
}
inline Item5_IDX merge_item4_IDX(const Item4_IDX &a, const Item4_IDX &b)
{
    return merge_item_generic<Item4_IDX, Item5_IDX>(a, b, ELL_BITS_144_5);
}

// Convenience aliases for disk helpers
using EquihashIPDiskWriter = IPDiskWriter<Item_IP>;
using EquihashIPDiskReader = IPDiskReader<Item_IP>;

// -----------------------------------------------------------------------------
// Wrapper helpers
// -----------------------------------------------------------------------------
inline void merge0_ip_inplace(Layer0_IDX &s, Layer1_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item0_IDX, Item1_IDX, Item_IP,
                             merge_item0_IDX, sort24<Item0_IDX>, false,
                             uint32_t, &getKey24<Item0_IDX>,
                             &is_zero_item<Item1_IDX>,
                             &make_ip_pair<Item0_IDX, Item_IP>>(s, d, ip);
}
inline void merge1_ip_inplace(Layer1_IDX &s, Layer2_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item1_IDX, Item2_IDX, Item_IP,
                             merge_item1_IDX, sort24<Item1_IDX>, false,
                             uint32_t, &getKey24<Item1_IDX>,
                             &is_zero_item<Item2_IDX>,
                             &make_ip_pair<Item1_IDX, Item_IP>>(s, d, ip);
}
inline void merge2_ip_inplace(Layer2_IDX &s, Layer3_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item2_IDX, Item3_IDX, Item_IP,
                             merge_item2_IDX, sort24<Item2_IDX>, false,
                             uint32_t, &getKey24<Item2_IDX>,
                             &is_zero_item<Item3_IDX>,
                             &make_ip_pair<Item2_IDX, Item_IP>>(s, d, ip);
}
inline void merge3_ip_inplace(Layer3_IDX &s, Layer4_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item3_IDX, Item4_IDX, Item_IP,
                             merge_item3_IDX, sort24<Item3_IDX>, false,
                             uint32_t, &getKey24<Item3_IDX>,
                             &is_zero_item<Item4_IDX>,
                             &make_ip_pair<Item3_IDX, Item_IP>>(s, d, ip);
}
inline void merge4_ip_inplace(Layer4_IDX &s, Layer5_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item4_IDX, Item5_IDX, Item_IP,
                             merge_item4_IDX, sort24<Item4_IDX>, false,
                             uint32_t, &getKey24<Item4_IDX>,
                             &is_zero_item<Item5_IDX>,
                             &make_ip_pair<Item4_IDX, Item_IP>, true>(s, d, ip);
}

inline void merge0_inplace(Layer0 &s, Layer1 &d)
{
    merge_inplace_generic<Item0, Item1, merge_item0, sort24<Item0>, false,
                          uint32_t, &getKey24<Item0>,
                          &is_zero_item<Item1>>(s, d);
}
inline void merge1_inplace(Layer1 &s, Layer2 &d)
{
    merge_inplace_generic<Item1, Item2, merge_item1, sort24<Item1>, false,
                          uint32_t, &getKey24<Item1>,
                          &is_zero_item<Item2>>(s, d);
}
inline void merge2_inplace(Layer2 &s, Layer3 &d)
{
    merge_inplace_generic<Item2, Item3, merge_item2, sort24<Item2>, false,
                          uint32_t, &getKey24<Item2>,
                          &is_zero_item<Item3>>(s, d);
}
inline void merge3_inplace(Layer3 &s, Layer4 &d)
{
    merge_inplace_generic<Item3, Item4, merge_item3, sort24<Item3>, false,
                          uint32_t, &getKey24<Item3>,
                          &is_zero_item<Item4>>(s, d);
}
inline void merge4_inplace(Layer4 &s, Layer5 &d)
{
    merge_inplace_generic<Item4, Item5, merge_item4, sort24<Item4>, false,
                          uint32_t, &getKey24<Item4>,
                          &is_zero_item<Item5>, true>(s, d);
}

inline void merge0_inplace_for_ip(Layer0_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item0_IDX, Item1_IDX, Item_IP,
                                 merge_item0_IDX, sort24<Item0_IDX>, false,
                                 uint32_t, &getKey24<Item0_IDX>,
                                 &is_zero_item<Item1_IDX>,
                                 &make_ip_pair<Item0_IDX, Item_IP>>(s, d);
}
inline void merge1_inplace_for_ip(Layer1_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item1_IDX, Item2_IDX, Item_IP,
                                 merge_item1_IDX, sort24<Item1_IDX>, false,
                                 uint32_t, &getKey24<Item1_IDX>,
                                 &is_zero_item<Item2_IDX>,
                                 &make_ip_pair<Item1_IDX, Item_IP>>(s, d);
}
inline void merge2_inplace_for_ip(Layer2_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item2_IDX, Item3_IDX, Item_IP,
                                 merge_item2_IDX, sort24<Item2_IDX>, false,
                                 uint32_t, &getKey24<Item2_IDX>,
                                 &is_zero_item<Item3_IDX>,
                                 &make_ip_pair<Item2_IDX, Item_IP>>(s, d);
}
inline void merge3_inplace_for_ip(Layer3_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item3_IDX, Item4_IDX, Item_IP,
                                 merge_item3_IDX, sort24<Item3_IDX>, false,
                                 uint32_t, &getKey24<Item3_IDX>,
                                 &is_zero_item<Item4_IDX>,
                                 &make_ip_pair<Item3_IDX, Item_IP>>(s, d);
}
inline void merge4_inplace_for_ip(Layer4_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item4_IDX, Item5_IDX, Item_IP,
                                 merge_item4_IDX, sort48<Item4_IDX>, false,
                                 uint64_t, &getKey48<Item4_IDX>,
                                 static_cast<bool (*)(const Item5_IDX &)>(nullptr),
                                 &make_ip_pair<Item4_IDX, Item_IP>, true>(s, d);
}

inline void merge0_em_ip_inplace(Layer0_IDX &s, Layer1_IDX &d,
                                 EquihashIPDiskWriter &writer)
{
    merge_em_ip_inplace_generic<Item0_IDX, Item1_IDX, Item_IP,
                                merge_item0_IDX, sort24<Item0_IDX>, false,
                                uint32_t, &getKey24<Item0_IDX>,
                                &is_zero_item<Item1_IDX>,
                                &make_ip_pair<Item0_IDX, Item_IP>>(s, d, writer);
}
inline void merge1_em_ip_inplace(Layer1_IDX &s, Layer2_IDX &d,
                                 EquihashIPDiskWriter &writer)
{
    merge_em_ip_inplace_generic<Item1_IDX, Item2_IDX, Item_IP,
                                merge_item1_IDX, sort24<Item1_IDX>, false,
                                uint32_t, &getKey24<Item1_IDX>,
                                &is_zero_item<Item2_IDX>,
                                &make_ip_pair<Item1_IDX, Item_IP>>(s, d, writer);
}
inline void merge2_em_ip_inplace(Layer2_IDX &s, Layer3_IDX &d,
                                 EquihashIPDiskWriter &writer)
{
    merge_em_ip_inplace_generic<Item2_IDX, Item3_IDX, Item_IP,
                                merge_item2_IDX, sort24<Item2_IDX>, false,
                                uint32_t, &getKey24<Item2_IDX>,
                                &is_zero_item<Item3_IDX>,
                                &make_ip_pair<Item2_IDX, Item_IP>>(s, d, writer);
}
inline void merge3_em_ip_inplace(Layer3_IDX &s, Layer4_IDX &d,
                                 EquihashIPDiskWriter &writer)
{
    merge_em_ip_inplace_generic<Item3_IDX, Item4_IDX, Item_IP,
                                merge_item3_IDX, sort24<Item3_IDX>, false,
                                uint32_t, &getKey24<Item3_IDX>,
                                &is_zero_item<Item4_IDX>,
                                &make_ip_pair<Item3_IDX, Item_IP>>(s, d, writer);
}

// Template wrappers for indexed access
template<size_t I>
inline void merge_inplace(Layer<I> &s, Layer<I+1> &d) {
    if constexpr (I == 0) merge0_inplace(s, d);
    else if constexpr (I == 1) merge1_inplace(s, d);
    else if constexpr (I == 2) merge2_inplace(s, d);
    else if constexpr (I == 3) merge3_inplace(s, d);
    else if constexpr (I == 4) merge4_inplace(s, d);
}

template<size_t I>
inline void merge_ip_inplace(LayerIDX<I> &s, LayerIDX<I+1> &d, Layer_IP &ip) {
    if constexpr (I == 0) merge0_ip_inplace(s, d, ip);
    else if constexpr (I == 1) merge1_ip_inplace(s, d, ip);
    else if constexpr (I == 2) merge2_ip_inplace(s, d, ip);
    else if constexpr (I == 3) merge3_ip_inplace(s, d, ip);
    else if constexpr (I == 4) merge4_ip_inplace(s, d, ip);
}

template<size_t I>
inline void merge_inplace_for_ip(LayerIDX<I> &s, Layer_IP &d) {
    if constexpr (I == 0) merge0_inplace_for_ip(s, d);
    else if constexpr (I == 1) merge1_inplace_for_ip(s, d);
    else if constexpr (I == 2) merge2_inplace_for_ip(s, d);
    else if constexpr (I == 3) merge3_inplace_for_ip(s, d);
    else if constexpr (I == 4) merge4_inplace_for_ip(s, d);
}

template<size_t I>
inline void merge_em_ip_inplace(LayerIDX<I> &s, LayerIDX<I+1> &d, EquihashIPDiskWriter &writer) {
    if constexpr (I == 0) merge0_em_ip_inplace(s, d, writer);
    else if constexpr (I == 1) merge1_em_ip_inplace(s, d, writer);
    else if constexpr (I == 2) merge2_em_ip_inplace(s, d, writer);
    else if constexpr (I == 3) merge3_em_ip_inplace(s, d, writer);
}

// -----------------------------------------------------------------------------
// IV merge helpers for Equihash(144,5)
// -----------------------------------------------------------------------------

// merge_iv wrapper for specific layers
inline IV1 merge_iv0(const IV0 &a, const IV0 &b)
{
    return merge_iv<EquihashParams::kIndexBytes, 0>(a, b);
}
inline IV2 merge_iv1(const IV1 &a, const IV1 &b)
{
    return merge_iv<EquihashParams::kIndexBytes, 1>(a, b);
}
inline IV3 merge_iv2(const IV2 &a, const IV2 &b)
{
    return merge_iv<EquihashParams::kIndexBytes, 2>(a, b);
}
inline IV4 merge_iv3(const IV3 &a, const IV3 &b)
{
    return merge_iv<EquihashParams::kIndexBytes, 3>(a, b);
}
inline IV5 merge_iv4(const IV4 &a, const IV4 &b)
{
    return merge_iv<EquihashParams::kIndexBytes, 4>(a, b);
}

// Key extraction wrappers (seed, iv) -> key
inline uint32_t getKey24_IV0(int seed, const IV0 &iv) { return getKey24<0>(seed, iv); }
inline uint32_t getKey24_IV1(int seed, const IV1 &iv) { return getKey24<1>(seed, iv); }
inline uint32_t getKey24_IV2(int seed, const IV2 &iv) { return getKey24<2>(seed, iv); }
inline uint32_t getKey24_IV3(int seed, const IV3 &iv) { return getKey24<3>(seed, iv); }
inline uint64_t getKey48_IV4(int seed, const IV4 &iv) { return getKey48<4>(seed, iv); }

// Sort wrappers (layer, seed) -> void
inline void sort24_IV0(LayerIV<0> &layer, int seed) { sort24_IV<0>(layer, seed); }
inline void sort24_IV1(LayerIV<1> &layer, int seed) { sort24_IV<1>(layer, seed); }
inline void sort24_IV2(LayerIV<2> &layer, int seed) { sort24_IV<2>(layer, seed); }
inline void sort24_IV3(LayerIV<3> &layer, int seed) { sort24_IV<3>(layer, seed); }
inline void sort48_IV4(LayerIV<4> &layer, int seed) { sort48_IV<4>(layer, seed); }

// Optimized merge_iv_layer using CachedHasher
template <typename SrcIV, typename DstIV,
          DstIV (*merge_iv_func)(const SrcIV &, const SrcIV &),
          size_t Layer, size_t KeyBits, typename KeyType,
          bool discard_zero,
          bool (*is_zero_func)(int, const DstIV &) = nullptr,
          bool is_last = false>
inline void merge_iv_layer_optimized(LayerVec<SrcIV> &src_arr, LayerVec<DstIV> &dst_arr, int seed)
{
    static_assert(!discard_zero || is_zero_func != nullptr,
                  "Zero-check function required when discarding zeros");

    if (src_arr.empty())
        return;

    // Initialize Hasher once
    uint8_t headernonce[140];
    std::memset(headernonce, 0, sizeof(headernonce));
    uint32_t *nonce_ptr = reinterpret_cast<uint32_t *>(headernonce + 108);
    *nonce_ptr = seed;
    uint8_t dummy_nonce[32];
    std::memset(dummy_nonce, 0, sizeof(dummy_nonce));

    // ...existing code...
    ZcashEquihashHasher H;
    H.init_midstate(headernonce, sizeof(headernonce), dummy_nonce,
                    EquihashParams::N,
                    EquihashParams::K);
    
    CachedHasher cachedH(H);

    // Sort source array by collision key (using hasher)
    sort_iv_by_key_fast<Layer, KeyBits>(src_arr, cachedH);

    const size_t N = src_arr.size();
    size_t avail_dst = dst_arr.capacity() - dst_arr.size();

    // For discard_zero mode, we need a skip buffer to mark zero-pair items
    std::vector<uint8_t> skip_buf;
    if constexpr (discard_zero)
    {
        skip_buf.reserve(GROUP_BOUND);
    }

    size_t i = 0;
    while (i < N)
    {
        // Find group with same collision key
        const size_t group_start = i;
        const auto key0 = get_iv_key_bits_fast<Layer, KeyBits>(cachedH, src_arr[group_start]);
        i++;
        while (i < N && get_iv_key_bits_fast<Layer, KeyBits>(cachedH, src_arr[i]) == key0)
            ++i;
        const size_t group_end = i;
// ...existing code...
        const size_t group_size = group_end - group_start;

        if constexpr (discard_zero)
        {
            // Mark items that produce zero XOR when merged
            skip_buf.assign(group_size, 0);
            for (size_t j1 = group_start; j1 < group_end; ++j1)
            {
                if (skip_buf[j1 - group_start])
                    continue;
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    if (skip_buf[j2 - group_start])
                        continue;
                    DstIV out = merge_iv_func(src_arr[j1], src_arr[j2]);
                    if (is_zero_func(seed, out))
                    {
                        skip_buf[j2 - group_start] = 1;
                        continue;
                    }
                    if (dst_arr.size() >= avail_dst)
                        break;
                    dst_arr.push_back(out);
                }
                if (dst_arr.size() >= avail_dst)
                    break;
            }
        }
        else
        {
            // Skip large groups in final merge to avoid explosion
            if constexpr (is_last)
            {
                if (group_size > 3)
                    continue;
            }

            // Generate all pairs within the group
            for (size_t j1 = group_start; j1 < group_end; ++j1)
            {
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    if (dst_arr.size() >= avail_dst)
                        break;
                    dst_arr.push_back(merge_iv_func(src_arr[j1], src_arr[j2]));
                }
                if (dst_arr.size() >= avail_dst)
                    break;
            }
        }

        // Check if destination is full
        if (dst_arr.size() >= avail_dst)
            break;
    }
}

// IV merge wrapper functions
inline void merge0_iv_inplace(Layer0_IV &s, Layer1_IV &d, int seed)
{
    merge_iv_layer_optimized<IV0, IV1, merge_iv0, 0, EQ1445_COLLISION_BITS, uint32_t, false>(s, d, seed);
}
inline void merge1_iv_layer(Layer1_IV &s, Layer2_IV &d, int seed)
{
    merge_iv_layer_optimized<IV1, IV2, merge_iv1, 1, EQ1445_COLLISION_BITS, uint32_t, false>(s, d, seed);
}
inline void merge2_iv_layer(Layer2_IV &s, Layer3_IV &d, int seed)
{
    merge_iv_layer_optimized<IV2, IV3, merge_iv2, 2, EQ1445_COLLISION_BITS, uint32_t, false>(s, d, seed);
}
inline void merge3_iv_layer(Layer3_IV &s, Layer4_IV &d, int seed)
{
    merge_iv_layer_optimized<IV3, IV4, merge_iv3, 3, EQ1445_COLLISION_BITS, uint32_t, false>(s, d, seed);
}
inline void merge4_iv_layer(Layer4_IV &s, Layer5_IV &d, int seed)
{
    merge_iv_layer_optimized<IV4, IV5, merge_iv4, 4, EQ1445_FINAL_BITS, uint64_t, false, nullptr, true>(s, d, seed);
}

// Template wrapper for indexed IV merge access
template<size_t I>
inline void merge_iv_layer(LayerIV<I> &s, LayerIV<I+1> &d, int seed) {
    if constexpr (I == 0) merge0_iv_inplace(s, d, seed);
    else if constexpr (I == 1) merge1_iv_layer(s, d, seed);
    else if constexpr (I == 2) merge2_iv_layer(s, d, seed);
    else if constexpr (I == 3) merge3_iv_layer(s, d, seed);
    else if constexpr (I == 4) merge4_iv_layer(s, d, seed);
}

// =============================================================================
// IV_Key merge helpers for Equihash(144,5)
// =============================================================================

// merge_ivkey wrapper for specific layers
inline IVKey1_24 merge_ivkey0(const IVKey0_24 &a, const IVKey0_24 &b)
{
    return merge_ivkey<EquihashParams::kIndexBytes, 0, uint32_t>(a, b);
}
inline IVKey2_24 merge_ivkey1(const IVKey1_24 &a, const IVKey1_24 &b)
{
    return merge_ivkey<EquihashParams::kIndexBytes, 1, uint32_t>(a, b);
}
inline IVKey3_24 merge_ivkey2(const IVKey2_24 &a, const IVKey2_24 &b)
{
    return merge_ivkey<EquihashParams::kIndexBytes, 2, uint32_t>(a, b);
}
inline IVKey4_48 merge_ivkey3(const IVKey3_24 &a, const IVKey3_24 &b)
{
    // Note: KeyType changes from uint32_t to uint64_t at Layer 4
    IVKey4_48 result;
    constexpr std::size_t half_bytes = EquihashParams::kIndexBytes * (1ULL << 3);  // 8 indices * 4 bytes
    std::memcpy(result.indices, a.indices, half_bytes);
    std::memcpy(result.indices + half_bytes, b.indices, half_bytes);
    // result.key is NOT set - must be computed later by SetIV_Key
    return result;
}
inline IVKey5_48 merge_ivkey4(const IVKey4_48 &a, const IVKey4_48 &b)
{
    return merge_ivkey<EquihashParams::kIndexBytes, 4, uint64_t>(a, b);
}

// Key extraction wrappers for IV_Key (just return stored key, no computation)
inline uint32_t getKey24_IVKey0(int seed, const IVKey0_24 &ivkey) { return getKey_IVKey<0>(seed, ivkey); }
inline uint32_t getKey24_IVKey1(int seed, const IVKey1_24 &ivkey) { return getKey_IVKey<1>(seed, ivkey); }
inline uint32_t getKey24_IVKey2(int seed, const IVKey2_24 &ivkey) { return getKey_IVKey<2>(seed, ivkey); }
inline uint32_t getKey24_IVKey3(int seed, const IVKey3_24 &ivkey) { return getKey_IVKey<3>(seed, ivkey); }
inline uint64_t getKey48_IVKey4(int seed, const IVKey4_48 &ivkey) { return getKey_IVKey<4>(seed, ivkey); }

// Sort wrappers for IV_Key (uses precomputed keys)
inline void sort24_IVKey0(Layer0_IVKey &layer, int seed) { sort24_IVKey<0>(layer, seed); }
inline void sort24_IVKey1(Layer1_IVKey &layer, int seed) { sort24_IVKey<1>(layer, seed); }
inline void sort24_IVKey2(Layer2_IVKey &layer, int seed) { sort24_IVKey<2>(layer, seed); }
inline void sort24_IVKey3(Layer3_IVKey &layer, int seed) { sort24_IVKey<3>(layer, seed); }
inline void sort48_IVKey4(Layer4_IVKey &layer, int seed) { sort48_IVKey<4>(layer, seed); }

// IV_Key merge wrapper functions
inline void merge0_ivkey_layer(Layer0_IVKey &s, Layer1_IVKey &d, int seed)
{
    merge_ivkey_layer_generic<IVKey0_24, IVKey1_24,
                              merge_ivkey0, sort24_IVKey0, false,
                              uint32_t, getKey24_IVKey0,
                              static_cast<bool (*)(int, const IVKey1_24 &)>(nullptr)>(s, d, seed);
}
inline void merge1_ivkey_layer(Layer1_IVKey &s, Layer2_IVKey &d, int seed)
{
    merge_ivkey_layer_generic<IVKey1_24, IVKey2_24,
                              merge_ivkey1, sort24_IVKey1, false,
                              uint32_t, getKey24_IVKey1,
                              static_cast<bool (*)(int, const IVKey2_24 &)>(nullptr)>(s, d, seed);
}
inline void merge2_ivkey_layer(Layer2_IVKey &s, Layer3_IVKey &d, int seed)
{
    merge_ivkey_layer_generic<IVKey2_24, IVKey3_24,
                              merge_ivkey2, sort24_IVKey2, false,
                              uint32_t, getKey24_IVKey2,
                              static_cast<bool (*)(int, const IVKey3_24 &)>(nullptr)>(s, d, seed);
}
inline void merge3_ivkey_layer(Layer3_IVKey &s, Layer4_IVKey &d, int seed)
{
    merge_ivkey_layer_generic<IVKey3_24, IVKey4_48,
                              merge_ivkey3, sort24_IVKey3, false,
                              uint32_t, getKey24_IVKey3,
                              static_cast<bool (*)(int, const IVKey4_48 &)>(nullptr)>(s, d, seed);
}
inline void merge4_ivkey_layer(Layer4_IVKey &s, Layer5_IVKey &d, int seed)
{
    merge_ivkey_layer_generic<IVKey4_48, IVKey5_48,
                              merge_ivkey4, sort48_IVKey4, false,
                              uint64_t, getKey48_IVKey4,
                              static_cast<bool (*)(int, const IVKey5_48 &)>(nullptr),
                              true>(s, d, seed);
}

// Template wrapper for indexed IV_Key merge access
template<size_t I>
inline void merge_ivkey_layer(Layer0_IVKey &s0, Layer1_IVKey &d1,
                              Layer1_IVKey &s1, Layer2_IVKey &d2,
                              Layer2_IVKey &s2, Layer3_IVKey &d3,
                              Layer3_IVKey &s3, Layer4_IVKey &d4,
                              Layer4_IVKey &s4, Layer5_IVKey &d5,
                              int seed) {
    if constexpr (I == 0) merge0_ivkey_layer(s0, d1, seed);
    else if constexpr (I == 1) merge1_ivkey_layer(s1, d2, seed);
    else if constexpr (I == 2) merge2_ivkey_layer(s2, d3, seed);
    else if constexpr (I == 3) merge3_ivkey_layer(s3, d4, seed);
    else if constexpr (I == 4) merge4_ivkey_layer(s4, d5, seed);
}
