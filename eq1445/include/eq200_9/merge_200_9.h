#pragma once

#include "eq200_9/equihash_200_9.h"
#include "core/merge.h"
#include "eq200_9/sort_200_9.h"

inline constexpr int ELL_BITS_200_9 =
    static_cast<int>(EquihashParams::kCollisionBitLength);

// -----------------------------------------------------------------------------
// Item merge helpers (parameter-specialized)
// -----------------------------------------------------------------------------
inline Item1_IDX merge_item0_IDX(const Item0_IDX &a, const Item0_IDX &b)
{
    return merge_item_generic<Item0_IDX, Item1_IDX>(a, b, ELL_BITS_200_9);
}
inline Item2_IDX merge_item1_IDX(const Item1_IDX &a, const Item1_IDX &b)
{
    return merge_item_generic<Item1_IDX, Item2_IDX>(a, b, ELL_BITS_200_9);
}
inline Item3_IDX merge_item2_IDX(const Item2_IDX &a, const Item2_IDX &b)
{
    return merge_item_generic<Item2_IDX, Item3_IDX>(a, b, ELL_BITS_200_9);
}
inline Item4_IDX merge_item3_IDX(const Item3_IDX &a, const Item3_IDX &b)
{
    return merge_item_generic<Item3_IDX, Item4_IDX>(a, b, ELL_BITS_200_9);
}
inline Item5_IDX merge_item4_IDX(const Item4_IDX &a, const Item4_IDX &b)
{
    return merge_item_generic<Item4_IDX, Item5_IDX>(a, b, ELL_BITS_200_9);
}
inline Item6_IDX merge_item5_IDX(const Item5_IDX &a, const Item5_IDX &b)
{
    return merge_item_generic<Item5_IDX, Item6_IDX>(a, b, ELL_BITS_200_9);
}
inline Item7_IDX merge_item6_IDX(const Item6_IDX &a, const Item6_IDX &b)
{
    return merge_item_generic<Item6_IDX, Item7_IDX>(a, b, ELL_BITS_200_9);
}
inline Item8_IDX merge_item7_IDX(const Item7_IDX &a, const Item7_IDX &b)
{
    return merge_item_generic<Item7_IDX, Item8_IDX>(a, b, ELL_BITS_200_9);
}
inline Item9_IDX merge_item8_IDX(const Item8_IDX &a, const Item8_IDX &b)
{
    return merge_item_generic<Item8_IDX, Item9_IDX>(a, b, ELL_BITS_200_9);
}

inline Item1 merge_item0(const Item0 &a, const Item0 &b)
{
    return merge_item_generic<Item0, Item1>(a, b, ELL_BITS_200_9);
}
inline Item2 merge_item1(const Item1 &a, const Item1 &b)
{
    return merge_item_generic<Item1, Item2>(a, b, ELL_BITS_200_9);
}
inline Item3 merge_item2(const Item2 &a, const Item2 &b)
{
    return merge_item_generic<Item2, Item3>(a, b, ELL_BITS_200_9);
}
inline Item4 merge_item3(const Item3 &a, const Item3 &b)
{
    return merge_item_generic<Item3, Item4>(a, b, ELL_BITS_200_9);
}
inline Item5 merge_item4(const Item4 &a, const Item4 &b)
{
    return merge_item_generic<Item4, Item5>(a, b, ELL_BITS_200_9);
}
inline Item6 merge_item5(const Item5 &a, const Item5 &b)
{
    return merge_item_generic<Item5, Item6>(a, b, ELL_BITS_200_9);
}
inline Item7 merge_item6(const Item6 &a, const Item6 &b)
{
    return merge_item_generic<Item6, Item7>(a, b, ELL_BITS_200_9);
}
inline Item8 merge_item7(const Item7 &a, const Item7 &b)
{
    return merge_item_generic<Item7, Item8>(a, b, ELL_BITS_200_9);
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
                             merge_item0_IDX, sort20<Item0_IDX>, true,
                             uint32_t, &getKey20<Item0_IDX>,
                             &is_zero_item<Item1_IDX>,
                             &make_ip_pair<Item0_IDX, Item_IP>>(s, d, ip);
}
inline void merge1_ip_inplace(Layer1_IDX &s, Layer2_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item1_IDX, Item2_IDX, Item_IP,
                             merge_item1_IDX, sort20<Item1_IDX>, true,
                             uint32_t, &getKey20<Item1_IDX>,
                             &is_zero_item<Item2_IDX>,
                             &make_ip_pair<Item1_IDX, Item_IP>>(s, d, ip);
}
inline void merge2_ip_inplace(Layer2_IDX &s, Layer3_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item2_IDX, Item3_IDX, Item_IP,
                             merge_item2_IDX, sort20<Item2_IDX>, true,
                             uint32_t, &getKey20<Item2_IDX>,
                             &is_zero_item<Item3_IDX>,
                             &make_ip_pair<Item2_IDX, Item_IP>>(s, d, ip);
}
inline void merge3_ip_inplace(Layer3_IDX &s, Layer4_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item3_IDX, Item4_IDX, Item_IP,
                             merge_item3_IDX, sort20<Item3_IDX>, true,
                             uint32_t, &getKey20<Item3_IDX>,
                             &is_zero_item<Item4_IDX>,
                             &make_ip_pair<Item3_IDX, Item_IP>>(s, d, ip);
}
inline void merge4_ip_inplace(Layer4_IDX &s, Layer5_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item4_IDX, Item5_IDX, Item_IP,
                             merge_item4_IDX, sort20<Item4_IDX>, true,
                             uint32_t, &getKey20<Item4_IDX>,
                             &is_zero_item<Item5_IDX>,
                             &make_ip_pair<Item4_IDX, Item_IP>>(s, d, ip);
}
inline void merge5_ip_inplace(Layer5_IDX &s, Layer6_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item5_IDX, Item6_IDX, Item_IP,
                             merge_item5_IDX, sort20<Item5_IDX>, true,
                             uint32_t, &getKey20<Item5_IDX>,
                             &is_zero_item<Item6_IDX>,
                             &make_ip_pair<Item5_IDX, Item_IP>>(s, d, ip);
}
inline void merge6_ip_inplace(Layer6_IDX &s, Layer7_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item6_IDX, Item7_IDX, Item_IP,
                             merge_item6_IDX, sort20<Item6_IDX>, true,
                             uint32_t, &getKey20<Item6_IDX>,
                             &is_zero_item<Item7_IDX>,
                             &make_ip_pair<Item6_IDX, Item_IP>>(s, d, ip);
}
inline void merge7_ip_inplace(Layer7_IDX &s, Layer8_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item7_IDX, Item8_IDX, Item_IP,
                             merge_item7_IDX, sort20<Item7_IDX>, true,
                             uint32_t, &getKey20<Item7_IDX>,
                             &is_zero_item<Item8_IDX>,
                             &make_ip_pair<Item7_IDX, Item_IP>>(s, d, ip);
}
inline void merge8_ip_inplace(Layer8_IDX &s, Layer9_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item8_IDX, Item9_IDX, Item_IP,
                             merge_item8_IDX, sort40<Item8_IDX>, false,
                             uint64_t, &getKey40<Item8_IDX>,
                             static_cast<bool (*)(const Item9_IDX &)>(nullptr),
                             &make_ip_pair<Item8_IDX, Item_IP>>(s, d, ip);
}

inline void merge0_inplace(Layer0 &s, Layer1 &d)
{
    merge_inplace_generic<Item0, Item1, merge_item0, sort20<Item0>, true,
                          uint32_t, &getKey20<Item0>,
                          &is_zero_item<Item1>>(s, d);
}
inline void merge1_inplace(Layer1 &s, Layer2 &d)
{
    merge_inplace_generic<Item1, Item2, merge_item1, sort20<Item1>, true,
                          uint32_t, &getKey20<Item1>,
                          &is_zero_item<Item2>>(s, d);
}
inline void merge2_inplace(Layer2 &s, Layer3 &d)
{
    merge_inplace_generic<Item2, Item3, merge_item2, sort20<Item2>, true,
                          uint32_t, &getKey20<Item2>,
                          &is_zero_item<Item3>>(s, d);
}
inline void merge3_inplace(Layer3 &s, Layer4 &d)
{
    merge_inplace_generic<Item3, Item4, merge_item3, sort20<Item3>, true,
                          uint32_t, &getKey20<Item3>,
                          &is_zero_item<Item4>>(s, d);
}
inline void merge4_inplace(Layer4 &s, Layer5 &d)
{
    merge_inplace_generic<Item4, Item5, merge_item4, sort20<Item4>, true,
                          uint32_t, &getKey20<Item4>,
                          &is_zero_item<Item5>>(s, d);
}
inline void merge5_inplace(Layer5 &s, Layer6 &d)
{
    merge_inplace_generic<Item5, Item6, merge_item5, sort20<Item5>, true,
                          uint32_t, &getKey20<Item5>,
                          &is_zero_item<Item6>>(s, d);
}
inline void merge6_inplace(Layer6 &s, Layer7 &d)
{
    merge_inplace_generic<Item6, Item7, merge_item6, sort20<Item6>, true,
                          uint32_t, &getKey20<Item6>,
                          &is_zero_item<Item7>>(s, d);
}
inline void merge7_inplace(Layer7 &s, Layer8 &d)
{
    merge_inplace_generic<Item7, Item8, merge_item7, sort20<Item7>, true,
                          uint32_t, &getKey20<Item7>,
                          &is_zero_item<Item8>>(s, d);
}

inline void merge0_inplace_for_ip(Layer0_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item0_IDX, Item1_IDX, Item_IP,
                                 merge_item0_IDX, sort20<Item0_IDX>, true,
                                 uint32_t, &getKey20<Item0_IDX>,
                                 &is_zero_item<Item1_IDX>,
                                 &make_ip_pair<Item0_IDX, Item_IP>>(s, d);
}
inline void merge1_inplace_for_ip(Layer1_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item1_IDX, Item2_IDX, Item_IP,
                                 merge_item1_IDX, sort20<Item1_IDX>, true,
                                 uint32_t, &getKey20<Item1_IDX>,
                                 &is_zero_item<Item2_IDX>,
                                 &make_ip_pair<Item1_IDX, Item_IP>>(s, d);
}
inline void merge2_inplace_for_ip(Layer2_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item2_IDX, Item3_IDX, Item_IP,
                                 merge_item2_IDX, sort20<Item2_IDX>, true,
                                 uint32_t, &getKey20<Item2_IDX>,
                                 &is_zero_item<Item3_IDX>,
                                 &make_ip_pair<Item2_IDX, Item_IP>>(s, d);
}
inline void merge3_inplace_for_ip(Layer3_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item3_IDX, Item4_IDX, Item_IP,
                                 merge_item3_IDX, sort20<Item3_IDX>, true,
                                 uint32_t, &getKey20<Item3_IDX>,
                                 &is_zero_item<Item4_IDX>,
                                 &make_ip_pair<Item3_IDX, Item_IP>>(s, d);
}
inline void merge4_inplace_for_ip(Layer4_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item4_IDX, Item5_IDX, Item_IP,
                                 merge_item4_IDX, sort20<Item4_IDX>, true,
                                 uint32_t, &getKey20<Item4_IDX>,
                                 &is_zero_item<Item5_IDX>,
                                 &make_ip_pair<Item4_IDX, Item_IP>>(s, d);
}
inline void merge5_inplace_for_ip(Layer5_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item5_IDX, Item6_IDX, Item_IP,
                                 merge_item5_IDX, sort20<Item5_IDX>, true,
                                 uint32_t, &getKey20<Item5_IDX>,
                                 &is_zero_item<Item6_IDX>,
                                 &make_ip_pair<Item5_IDX, Item_IP>>(s, d);
}
inline void merge6_inplace_for_ip(Layer6_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item6_IDX, Item7_IDX, Item_IP,
                                 merge_item6_IDX, sort20<Item6_IDX>, true,
                                 uint32_t, &getKey20<Item6_IDX>,
                                 &is_zero_item<Item7_IDX>,
                                 &make_ip_pair<Item6_IDX, Item_IP>>(s, d);
}
inline void merge7_inplace_for_ip(Layer7_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item7_IDX, Item8_IDX, Item_IP,
                                 merge_item7_IDX, sort20<Item7_IDX>, true,
                                 uint32_t, &getKey20<Item7_IDX>,
                                 &is_zero_item<Item8_IDX>,
                                 &make_ip_pair<Item7_IDX, Item_IP>>(s, d);
}
inline void merge8_inplace_for_ip(Layer8_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item8_IDX, Item9_IDX, Item_IP,
                                 merge_item8_IDX, sort40<Item8_IDX>, false,
                                 uint64_t, &getKey40<Item8_IDX>,
                                 static_cast<bool (*)(const Item9_IDX &)>(nullptr),
                                 &make_ip_pair<Item8_IDX, Item_IP>>(s, d);
}

inline void merge0_em_ip_inplace(Layer0_IDX &s, Layer1_IDX &d,
                                 EquihashIPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item0_IDX, Item1_IDX, Item_IP,
                                merge_item0_IDX, sort20<Item0_IDX>, true,
                                uint32_t, &getKey20<Item0_IDX>,
                                &is_zero_item<Item1_IDX>,
                                &make_ip_pair<Item0_IDX, Item_IP>>(s, d, w);
}
inline void merge1_em_ip_inplace(Layer1_IDX &s, Layer2_IDX &d,
                                 EquihashIPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item1_IDX, Item2_IDX, Item_IP,
                                merge_item1_IDX, sort20<Item1_IDX>, true,
                                uint32_t, &getKey20<Item1_IDX>,
                                &is_zero_item<Item2_IDX>,
                                &make_ip_pair<Item1_IDX, Item_IP>>(s, d, w);
}
inline void merge2_em_ip_inplace(Layer2_IDX &s, Layer3_IDX &d,
                                 EquihashIPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item2_IDX, Item3_IDX, Item_IP,
                                merge_item2_IDX, sort20<Item2_IDX>, true,
                                uint32_t, &getKey20<Item2_IDX>,
                                &is_zero_item<Item3_IDX>,
                                &make_ip_pair<Item2_IDX, Item_IP>>(s, d, w);
}
inline void merge3_em_ip_inplace(Layer3_IDX &s, Layer4_IDX &d,
                                 EquihashIPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item3_IDX, Item4_IDX, Item_IP,
                                merge_item3_IDX, sort20<Item3_IDX>, true,
                                uint32_t, &getKey20<Item3_IDX>,
                                &is_zero_item<Item4_IDX>,
                                &make_ip_pair<Item3_IDX, Item_IP>>(s, d, w);
}
inline void merge4_em_ip_inplace(Layer4_IDX &s, Layer5_IDX &d,
                                 EquihashIPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item4_IDX, Item5_IDX, Item_IP,
                                merge_item4_IDX, sort20<Item4_IDX>, true,
                                uint32_t, &getKey20<Item4_IDX>,
                                &is_zero_item<Item5_IDX>,
                                &make_ip_pair<Item4_IDX, Item_IP>>(s, d, w);
}
inline void merge5_em_ip_inplace(Layer5_IDX &s, Layer6_IDX &d,
                                 EquihashIPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item5_IDX, Item6_IDX, Item_IP,
                                merge_item5_IDX, sort20<Item5_IDX>, true,
                                uint32_t, &getKey20<Item5_IDX>,
                                &is_zero_item<Item6_IDX>,
                                &make_ip_pair<Item5_IDX, Item_IP>>(s, d, w);
}
inline void merge6_em_ip_inplace(Layer6_IDX &s, Layer7_IDX &d,
                                 EquihashIPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item6_IDX, Item7_IDX, Item_IP,
                                merge_item6_IDX, sort20<Item6_IDX>, true,
                                uint32_t, &getKey20<Item6_IDX>,
                                &is_zero_item<Item7_IDX>,
                                &make_ip_pair<Item6_IDX, Item_IP>>(s, d, w);
}
inline void merge7_em_ip_inplace(Layer7_IDX &s, Layer8_IDX &d,
                                 EquihashIPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item7_IDX, Item8_IDX, Item_IP,
                                merge_item7_IDX, sort20<Item7_IDX>, true,
                                uint32_t, &getKey20<Item7_IDX>,
                                &is_zero_item<Item8_IDX>,
                                &make_ip_pair<Item7_IDX, Item_IP>>(s, d, w);
}
inline void merge8_em_ip_inplace(Layer8_IDX &s, Layer9_IDX &d,
                                 EquihashIPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item8_IDX, Item9_IDX, Item_IP,
                                merge_item8_IDX, sort40<Item8_IDX>, false,
                                uint64_t, &getKey40<Item8_IDX>,
                                static_cast<bool (*)(const Item9_IDX &)>(nullptr),
                                &make_ip_pair<Item8_IDX, Item_IP>>(s, d, w);
}
