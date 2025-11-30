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

// IV merge wrapper functions
inline void merge0_iv_inplace(Layer0_IV &s, Layer1_IV &d, int seed)
{
    merge_iv_layer_generic<IV0, IV1,
                             merge_iv0, sort24_IV0, false,
                             uint32_t, getKey24_IV0,
                             static_cast<bool (*)(int, const IV1 &)>(nullptr)>(s, d, seed);
}
inline void merge1_iv_layer(Layer1_IV &s, Layer2_IV &d, int seed)
{
    merge_iv_layer_generic<IV1, IV2,
                             merge_iv1, sort24_IV1, false,
                             uint32_t, getKey24_IV1,
                             static_cast<bool (*)(int, const IV2 &)>(nullptr)>(s, d, seed);
}
inline void merge2_iv_layer(Layer2_IV &s, Layer3_IV &d, int seed)
{
    merge_iv_layer_generic<IV2, IV3,
                             merge_iv2, sort24_IV2, false,
                             uint32_t, getKey24_IV2,
                             static_cast<bool (*)(int, const IV3 &)>(nullptr)>(s, d, seed);
}
inline void merge3_iv_layer(Layer3_IV &s, Layer4_IV &d, int seed)
{
    merge_iv_layer_generic<IV3, IV4,
                             merge_iv3, sort24_IV3, false,
                             uint32_t, getKey24_IV3,
                             static_cast<bool (*)(int, const IV4 &)>(nullptr)>(s, d, seed);
}
inline void merge4_iv_layer(Layer4_IV &s, Layer5_IV &d, int seed)
{
    merge_iv_layer_generic<IV4, IV5,
                             merge_iv4, sort48_IV4, false,
                             uint64_t, getKey48_IV4,
                             static_cast<bool (*)(int, const IV5 &)>(nullptr),
                             true>(s, d, seed);
}

// Template wrapper for indexed IV merge access
template<size_t I>
inline void merge_iv_inplace(LayerIV<I> &s, LayerIV<I+1> &d, int seed) {
    if constexpr (I == 0) merge0_iv_inplace(s, d, seed);
    else if constexpr (I == 1) merge1_iv_inplace(s, d, seed);
    else if constexpr (I == 2) merge2_iv_inplace(s, d, seed);
    else if constexpr (I == 3) merge3_iv_inplace(s, d, seed);
    else if constexpr (I == 4) merge4_iv_inplace(s, d, seed);
}
