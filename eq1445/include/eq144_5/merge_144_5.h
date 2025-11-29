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
