#pragma once

#include <algorithm>
#include <deque>
#include <cstdint>
#include <cstring>
#include <vector>
#include "layer.h"
#include "sort.h"
#include "util.h"
#include "zcash_blake.h"

#define MOVE_BOUND 2048
#define TMP_SIZE 1024
#define GROUP_BOUND 256
#define ELL_BITS 20

// ------------------ XOR shift & merge helpers ------------------
template <size_t NI, size_t NO>
inline void xor_shift_right_u8(uint8_t (&dst)[NO], const uint8_t (&a)[NI],
                               const uint8_t (&b)[NI],
                               int shift_bits = ELL_BITS)
{
    uint8_t tmp[NI];
    for (size_t i = 0; i < NI; ++i)
        tmp[i] = a[i] ^ b[i];
    const int by = shift_bits / 8, bt = shift_bits % 8;
    const uint8_t *s = tmp + by;

    for (size_t i = 0; i + 1 < NO; ++i)
    {
        dst[i] = uint8_t((s[i] >> bt) | (s[i + 1] << (8 - bt)));
    }
    if (by + NO < NI)
    {
        dst[NO - 1] = uint8_t((s[NO - 1] >> bt) | (s[NO] << (8 - bt)));
    }
    else
    {
        dst[NO - 1] = uint8_t(s[NO - 1] >> bt);
    }
}

template <typename Src, typename Dst>
inline Dst merge_item_generic(const Src &a, const Src &b,
                              int shift_bits = ELL_BITS)
{
    Dst out;
    xor_shift_right_u8<sizeof(a.XOR), sizeof(out.XOR)>(out.XOR, a.XOR, b.XOR,
                                                       shift_bits);
    return out;
}

// Indexed merge helpers
inline Item1_IDX merge_item0_IDX(const Item0_IDX &a, const Item0_IDX &b)
{
    return merge_item_generic<Item0_IDX, Item1_IDX>(a, b);
}
inline Item2_IDX merge_item1_IDX(const Item1_IDX &a, const Item1_IDX &b)
{
    return merge_item_generic<Item1_IDX, Item2_IDX>(a, b);
}
inline Item3_IDX merge_item2_IDX(const Item2_IDX &a, const Item2_IDX &b)
{
    return merge_item_generic<Item2_IDX, Item3_IDX>(a, b);
}
inline Item4_IDX merge_item3_IDX(const Item3_IDX &a, const Item3_IDX &b)
{
    return merge_item_generic<Item3_IDX, Item4_IDX>(a, b);
}
inline Item5_IDX merge_item4_IDX(const Item4_IDX &a, const Item4_IDX &b)
{
    return merge_item_generic<Item4_IDX, Item5_IDX>(a, b);
}
inline Item6_IDX merge_item5_IDX(const Item5_IDX &a, const Item5_IDX &b)
{
    return merge_item_generic<Item5_IDX, Item6_IDX>(a, b);
}
inline Item7_IDX merge_item6_IDX(const Item6_IDX &a, const Item6_IDX &b)
{
    return merge_item_generic<Item6_IDX, Item7_IDX>(a, b);
}
inline Item8_IDX merge_item7_IDX(const Item7_IDX &a, const Item7_IDX &b)
{
    return merge_item_generic<Item7_IDX, Item8_IDX>(a, b);
}
inline Item9_IDX merge_item8_IDX(const Item8_IDX &a, const Item8_IDX &b)
{
    return merge_item_generic<Item8_IDX, Item9_IDX>(a, b);
}

// Non-indexed merge helpers
inline Item1 merge_item0(const Item0 &a, const Item0 &b)
{
    return merge_item_generic<Item0, Item1>(a, b);
}
inline Item2 merge_item1(const Item1 &a, const Item1 &b)
{
    return merge_item_generic<Item1, Item2>(a, b);
}
inline Item3 merge_item2(const Item2 &a, const Item2 &b)
{
    return merge_item_generic<Item2, Item3>(a, b);
}
inline Item4 merge_item3(const Item3 &a, const Item3 &b)
{
    return merge_item_generic<Item3, Item4>(a, b);
}
inline Item5 merge_item4(const Item4 &a, const Item4 &b)
{
    return merge_item_generic<Item4, Item5>(a, b);
}
inline Item6 merge_item5(const Item5 &a, const Item5 &b)
{
    return merge_item_generic<Item5, Item6>(a, b);
}
inline Item7 merge_item6(const Item6 &a, const Item6 &b)
{
    return merge_item_generic<Item6, Item7>(a, b);
}
inline Item8 merge_item7(const Item7 &a, const Item7 &b)
{
    return merge_item_generic<Item7, Item8>(a, b);
}

template <typename SrcWithIdx>
inline Item_IP make_ip_pair(const SrcWithIdx &a, const SrcWithIdx &b)
{
    Item_IP ip{};
    for (int i = 0; i < 3; ++i)
    {
        ip.index_pointer_left[i] = a.index[i];
        ip.index_pointer_right[i] = b.index[i];
    }
    return ip;
}

template <typename ItemType>
inline bool is_zero_item(const ItemType &item)
{
    // check only 40 bits (5 bytes) of XOR field
    for (size_t i = 0; i < 5; ++i)
    {
        if (item.XOR[i] != 0)
        {
            return false;
        }
    }
    return true;
}

template <typename T>
void drain_deque_to_vector(std::deque<T> &dq, LayerVec<T> &out, std::size_t t)
{
    // assert(t <= dq.size());
    if (t == 0)
        return;
    out.insert(out.end(), std::make_move_iterator(dq.begin()),
               std::make_move_iterator(dq.begin() + t));
    dq.erase(dq.begin(), dq.begin() + t);
}

template <typename T>
void drain_vectors(std::vector<T> &src_arr, LayerVec<T> &dst_arr, std::size_t t)
{
    // move the first t elements from `src_arr` to `dst_arr`.
    // after this operation, if src_arr.size() == t, then src_arr will be empty.
    // otherwise, move the remaining elements to the front of src_arr, the final size will be src_arr.size() - t.
    // src_arr.capacity() and dst_arr.capacity() is large enuogh for this operation (ensured by the caller).
    // assert(t <= src_arr.size());
    if (t == 0)
        return;

    dst_arr.insert(
        dst_arr.end(),
        std::make_move_iterator(src_arr.begin()),
        std::make_move_iterator(src_arr.begin() + t));

    const std::size_t original_size = src_arr.size();
    if (t < original_size)
    {
        std::move(src_arr.begin() + t, src_arr.end(), src_arr.begin());
        src_arr.resize(original_size - t);
    }
    else
    {
        src_arr.clear();
    }
}

// ------------------ Merge (in-place) with IP capture ------------------
// `src_arr` and `dst_arr` share the same memory region for memory-efficiency.
// `ip_arr` does not share the memory with `dst_arr` to simplify management
template <typename SrcItem, typename DstItem,
          DstItem (*merge_func)(const SrcItem &, const SrcItem &),
          void (*sort_func)(LayerVec<SrcItem> &), bool discard_zero,
          typename KeyType = uint32_t,
          KeyType (*key_func)(const SrcItem &) = getKey20<SrcItem>,
          const size_t move_bound = MOVE_BOUND, const size_t max_tmp_size = TMP_SIZE>
inline void merge_ip_inplace_generic(LayerVec<SrcItem> &src_arr,
                                     LayerVec<DstItem> &dst_arr,
                                     Layer_IP &ip_arr)
{
    if (src_arr.empty())
        return;
    // assert(dst_arr.capacity() > 0 && ip_arr.capacity() > 0);
    // assert(dst_arr.size() == 0 && ip_arr.size() == 0);
    // assert(dst_arr.capacity() == ip_arr.capacity());
    sort_func(src_arr);
    const size_t N = src_arr.size();
    // const size_t sz_src = sizeof(SrcItem), sz_dst = std::max(sizeof(DstItem), sizeof(Item_IP));
    const size_t sz_src = sizeof(SrcItem), sz_dst = sizeof(DstItem);

    // We can also use a FIFO queue so that items are emitted in the same order
    std::vector<DstItem> tmp_items;
    std::vector<Item_IP> tmp_ips;
    std::vector<uint8_t> skip_buf;
    tmp_items.reserve(max_tmp_size);
    tmp_ips.reserve(max_tmp_size);
    skip_buf.reserve(GROUP_BOUND);
    size_t free_bytes = 0;
    size_t i = 0;
    size_t avail_dst = dst_arr.capacity() - dst_arr.size();
    while (i < N)
    {
        const size_t group_start = i;
        const auto key0 = key_func(src_arr[group_start]);
        i++;
        while (i < N && key_func(src_arr[i]) == key0)
            ++i;
        const size_t group_end = i;
        const size_t group_size = group_end - group_start;
        if (discard_zero)
        {
            // Reuse skip_buf to avoid per-group allocations.
            skip_buf.assign(group_size, 0);
            for (size_t j1 = group_start; j1 < group_end; ++j1)
            {
                if (skip_buf[j1 - group_start])
                    continue;
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    if (skip_buf[j2 - group_start])
                        continue;
                    DstItem out = merge_func(src_arr[j1], src_arr[j2]);
                    if (is_zero_item(out))
                    {
                        skip_buf[j2 - group_start] = 1;
                        continue;
                    }
                    tmp_items.emplace_back(out);
                    tmp_ips.emplace_back(make_ip_pair(src_arr[j1], src_arr[j2]));
                }
            }
        }
        else
        {
            if (group_size > 3)
            {
                continue;
            } // last hop: skip large groups since they are trivial solutions with overwhelming prob.
            for (size_t j1 = group_start; j1 < group_end; ++j1)
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    tmp_items.emplace_back(merge_func(src_arr[j1], src_arr[j2]));
                    tmp_ips.emplace_back(make_ip_pair(src_arr[j1], src_arr[j2]));
                }
        }

        const size_t tmp_size = tmp_items.size();
        if (tmp_size >= avail_dst) // already full, we will throw away remaining tmp_items/tmp_ips
        {
            break;
        }
        free_bytes += group_size * sz_src;
        const size_t can_dst = free_bytes / sz_dst;
        const size_t to_move = std::min<size_t>({tmp_size, can_dst, avail_dst});
        if (to_move >= move_bound) // trade-off: to avoid too-frequent small moves, we only move when we have enough items.
        {
            drain_vectors(tmp_items, dst_arr, to_move);
            drain_vectors(tmp_ips, ip_arr, to_move);
            free_bytes -= to_move * sz_dst;
            avail_dst = dst_arr.capacity() - dst_arr.size();
        }
    }
    if (tmp_items.size())
    {
        // const size_t avail_dst = dst_arr.capacity() - dst_arr.size();
        const size_t to_move = std::min(tmp_items.size(), avail_dst);
        drain_vectors(tmp_items, dst_arr, to_move);
        drain_vectors(tmp_ips, ip_arr, to_move);
    }
}

// Short wrappers for each round (indexed)
inline void merge0_ip_inplace(Layer0_IDX &s, Layer1_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item0_IDX, Item1_IDX, merge_item0_IDX,
                             sort20<Item0_IDX>, true>(s, d, ip);
}
inline void merge1_ip_inplace(Layer1_IDX &s, Layer2_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item1_IDX, Item2_IDX, merge_item1_IDX,
                             sort20<Item1_IDX>, true>(s, d, ip);
}
inline void merge2_ip_inplace(Layer2_IDX &s, Layer3_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item2_IDX, Item3_IDX, merge_item2_IDX,
                             sort20<Item2_IDX>, true>(s, d, ip);
}
inline void merge3_ip_inplace(Layer3_IDX &s, Layer4_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item3_IDX, Item4_IDX, merge_item3_IDX,
                             sort20<Item3_IDX>, true>(s, d, ip);
}
inline void merge4_ip_inplace(Layer4_IDX &s, Layer5_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item4_IDX, Item5_IDX, merge_item4_IDX,
                             sort20<Item4_IDX>, true>(s, d, ip);
}
inline void merge5_ip_inplace(Layer5_IDX &s, Layer6_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item5_IDX, Item6_IDX, merge_item5_IDX,
                             sort20<Item5_IDX>, true>(s, d, ip);
}
inline void merge6_ip_inplace(Layer6_IDX &s, Layer7_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item6_IDX, Item7_IDX, merge_item6_IDX,
                             sort20<Item6_IDX>, true>(s, d, ip);
}
inline void merge7_ip_inplace(Layer7_IDX &s, Layer8_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item7_IDX, Item8_IDX, merge_item7_IDX,
                             sort20<Item7_IDX>, true>(s, d, ip);
}
inline void merge8_ip_inplace(Layer8_IDX &s, Layer9_IDX &d, Layer_IP &ip)
{
    merge_ip_inplace_generic<Item8_IDX, Item9_IDX, merge_item8_IDX,
                             sort40<Item8_IDX>, false, uint64_t,
                             getKey40<Item8_IDX>>(s, d, ip);
}

// ------------------ Merge (in-place) without IP capture ------------------
// `src_arr` and `dst_arr` share the same memory region for memory-efficiency.
template <typename SrcItem, typename DstItem,
          DstItem (*merge_func)(const SrcItem &, const SrcItem &),
          void (*sort_func)(LayerVec<SrcItem> &), bool discard_zero,
          typename KeyType = uint32_t,
          KeyType (*key_func)(const SrcItem &) = getKey20<SrcItem>,
          const size_t move_bound = MOVE_BOUND, const size_t max_tmp_size = TMP_SIZE>
inline void merge_inplace_generic(LayerVec<SrcItem> &src_arr,
                                  LayerVec<DstItem> &dst_arr)
{
    if (src_arr.empty())
        return;
    sort_func(src_arr);
    const size_t N = src_arr.size();
    // const size_t sz_src = sizeof(SrcItem), sz_dst = std::max(sizeof(DstItem), sizeof(Item_IP));
    const size_t sz_src = sizeof(SrcItem), sz_dst = sizeof(DstItem);

    // FIFO buffer for temporary items
    std::vector<DstItem> tmp_items;
    std::vector<uint8_t> skip_buf;
    skip_buf.reserve(GROUP_BOUND);
    size_t free_bytes = 0;
    size_t avail_dst = dst_arr.capacity() - dst_arr.size();

    size_t i = 0;
    while (i < N)
    {
        const size_t group_start = i;
        const auto key0 = key_func(src_arr[group_start]);
        i++;
        while (i < N && key_func(src_arr[i]) == key0)
            ++i;
        const size_t group_end = i;
        const size_t group_size = group_end - group_start;

        if (discard_zero)
        {
            skip_buf.assign(group_size, 0);
            for (size_t j1 = group_start; j1 < group_end; ++j1)
            {
                if (skip_buf[j1 - group_start])
                    continue;
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    if (skip_buf[j2 - group_start])
                        continue;
                    DstItem out = merge_func(src_arr[j1], src_arr[j2]);
                    if (is_zero_item(out))
                    {
                        skip_buf[j2 - group_start] = 1;
                        continue;
                    }
                    tmp_items.emplace_back(out);
                }
            }
        }
        else
        {
            if (group_size > 3)
            {
                continue;
            } // last hop: skip large groups since they are trivial solutions with overwhelming prob.
            for (size_t j1 = group_start; j1 < group_end; ++j1)
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    tmp_items.emplace_back(merge_func(src_arr[j1], src_arr[j2]));
                }
        }

        const size_t tmp_size = tmp_items.size();
        if (tmp_size >= avail_dst) // already full, we will throw away remaining tmp_items
        {
            break;
        }
        free_bytes += group_size * sz_src;
        const size_t can_dst = free_bytes / sz_dst;
        const size_t to_move = std::min<size_t>({tmp_size, can_dst, avail_dst});
        if (to_move >= move_bound) // trade-off: to avoid too-frequent small moves, we only move when we have enough items.
        {
            drain_vectors(tmp_items, dst_arr, to_move);
            free_bytes -= to_move * sz_dst;
            avail_dst = dst_arr.capacity() - dst_arr.size();
        }
    }

    if (tmp_items.size())
    {
        // const size_t avail_dst = dst_arr.capacity() - dst_arr.size();
        const size_t to_move = std::min(tmp_items.size(), avail_dst);
        drain_vectors(tmp_items, dst_arr, to_move);
    }
}

inline void merge0_inplace(Layer0 &s, Layer1 &d)
{
    merge_inplace_generic<Item0, Item1, merge_item0, sort20<Item0>, true>(s, d);
}
inline void merge1_inplace(Layer1 &s, Layer2 &d)
{
    merge_inplace_generic<Item1, Item2, merge_item1, sort20<Item1>, true>(s, d);
}
inline void merge2_inplace(Layer2 &s, Layer3 &d)
{
    merge_inplace_generic<Item2, Item3, merge_item2, sort20<Item2>, true>(s, d);
}
inline void merge3_inplace(Layer3 &s, Layer4 &d)
{
    merge_inplace_generic<Item3, Item4, merge_item3, sort20<Item3>, true>(s, d);
}
inline void merge4_inplace(Layer4 &s, Layer5 &d)
{
    merge_inplace_generic<Item4, Item5, merge_item4, sort20<Item4>, true>(s, d);
}
inline void merge5_inplace(Layer5 &s, Layer6 &d)
{
    merge_inplace_generic<Item5, Item6, merge_item5, sort20<Item5>, true>(s, d);
}
inline void merge6_inplace(Layer6 &s, Layer7 &d)
{
    merge_inplace_generic<Item6, Item7, merge_item6, sort20<Item6>, true>(s, d);
}
inline void merge7_inplace(Layer7 &s, Layer8 &d)
{
    merge_inplace_generic<Item7, Item8, merge_item7, sort20<Item7>, true>(s, d);
}

// ------------------ Merge (in-place): output only IP ------------------
// `src_arr` and `dst_arr` share the same memory region for memory-efficiency.
template <typename SrcItem, typename DstItem,
          DstItem (*merge_func)(const SrcItem &, const SrcItem &),
          void (*sort_func)(LayerVec<SrcItem> &), bool discard_zero,
          typename KeyType = uint32_t,
          KeyType (*key_func)(const SrcItem &) = getKey20<SrcItem>,
          const size_t move_bound = MOVE_BOUND, const size_t max_tmp_size = TMP_SIZE>
inline void merge_inplace_for_ip_generic(LayerVec<SrcItem> &src_arr,
                                         LayerVec<Item_IP> &dst_arr)
{
    if (src_arr.empty())
        return;
    sort_func(src_arr);
    const size_t N = src_arr.size();
    // const size_t sz_src = sizeof(SrcItem), sz_dst = std::max(sizeof(DstItem), sizeof(Item_IP));
    const size_t sz_src = sizeof(SrcItem), sz_dst = sizeof(Item_IP);

    // FIFO buffer for temporary IP items
    std::vector<Item_IP> tmp_items;
    std::vector<uint8_t> skip_buf;
    tmp_items.reserve(max_tmp_size);
    skip_buf.reserve(GROUP_BOUND);

    size_t free_bytes = 0;
    size_t avail_dst = dst_arr.capacity() - dst_arr.size();
    size_t i = 0;

    while (i < N)
    {
        const size_t group_start = i;
        const auto key0 = key_func(src_arr[group_start]);
        i++;
        while (i < N && key_func(src_arr[i]) == key0)
            ++i;
        const size_t group_end = i;
        const size_t group_size = group_end - group_start;

        if (discard_zero)
        {
            skip_buf.assign(group_size, 0);
            for (size_t j1 = group_start; j1 < group_end; ++j1)
            {
                if (skip_buf[j1 - group_start])
                    continue;
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    if (skip_buf[j2 - group_start])
                        continue;
                    DstItem out = merge_func(src_arr[j1], src_arr[j2]);
                    if (is_zero_item(out))
                    {
                        skip_buf[j2 - group_start] = 1;
                        continue;
                    }
                    tmp_items.emplace_back(make_ip_pair(src_arr[j1], src_arr[j2]));
                }
            }
        }
        else
        {
            if (group_size > 3)
            {
                continue;
            } // last hop: skip large groups since they are trivial solutions with overwhelming prob.
            for (size_t j1 = group_start; j1 < group_end; ++j1)
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    tmp_items.emplace_back(make_ip_pair(src_arr[j1], src_arr[j2]));
                }
        }

        const size_t tmp_size = tmp_items.size();
        if (tmp_size >= avail_dst) // already full, we will throw away remaining tmp_items
        {
            break;
        }
        free_bytes += group_size * sz_src;
        const size_t can_dst = free_bytes / sz_dst;
        const size_t to_move = std::min<size_t>({tmp_size, can_dst, avail_dst});
        if (to_move >= move_bound) // trade-off: to avoid too-frequent small moves, we only move when we have enough items.
        {
            drain_vectors(tmp_items, dst_arr, to_move);
            free_bytes -= to_move * sz_dst;
            avail_dst = dst_arr.capacity() - dst_arr.size();
        }
    }

    if (tmp_items.size())
    {
        // const size_t avail_dst = dst_arr.capacity() - dst_arr.size();
        const size_t to_move = std::min(tmp_items.size(), avail_dst);
        drain_vectors(tmp_items, dst_arr, to_move);
    }
}

inline void merge0_inplace_for_ip(Layer0_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item0_IDX, Item1_IDX, merge_item0_IDX, sort20<Item0_IDX>, true>(s, d);
}

inline void merge1_inplace_for_ip(Layer1_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item1_IDX, Item2_IDX, merge_item1_IDX, sort20<Item1_IDX>, true>(s, d);
}
inline void merge2_inplace_for_ip(Layer2_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item2_IDX, Item3_IDX, merge_item2_IDX, sort20<Item2_IDX>, true>(s, d);
}
inline void merge3_inplace_for_ip(Layer3_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item3_IDX, Item4_IDX, merge_item3_IDX, sort20<Item3_IDX>, true>(s, d);
}
inline void merge4_inplace_for_ip(Layer4_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item4_IDX, Item5_IDX, merge_item4_IDX, sort20<Item4_IDX>, true>(s, d);
}
inline void merge5_inplace_for_ip(Layer5_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item5_IDX, Item6_IDX, merge_item5_IDX, sort20<Item5_IDX>, true>(s, d);
}
inline void merge6_inplace_for_ip(Layer6_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item6_IDX, Item7_IDX, merge_item6_IDX, sort20<Item6_IDX>, true>(s, d);
}
inline void merge7_inplace_for_ip(Layer7_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item7_IDX, Item8_IDX, merge_item7_IDX, sort20<Item7_IDX>, true>(s, d);
}
inline void merge8_inplace_for_ip(Layer8_IDX &s, Layer_IP &d)
{
    merge_inplace_for_ip_generic<Item8_IDX, Item9_IDX, merge_item8_IDX, sort40<Item8_IDX>, false, uint64_t, getKey40<Item8_IDX>>(s, d);
}

// external memory classes

class IPDiskWriter
{
public:
    std::FILE *f_;
    uint64_t cursor_;

    IPDiskWriter() : f_(nullptr), cursor_(0) {}
    ~IPDiskWriter() { close(); }
    bool open(const char *p)
    {
        close();
        f_ = std::fopen(p, "wb");
        cursor_ = 0;
        return f_ != nullptr;
    }
    void close()
    {
        if (f_)
        {
            std::fclose(f_);
            f_ = nullptr;
        }
        cursor_ = 0;
    }

    uint64_t append_layer(const Item_IP *data, size_t n)
    {
        if (!f_ || !n)
            return cursor_;
        size_t bytes = n * sizeof(Item_IP);
        size_t w = std::fwrite(data, 1, bytes, f_);
        if (w != bytes)
            std::perror("fwrite");
        uint64_t off = cursor_;
        cursor_ += w;
        return off;
    }

    bool write_ip_item(const Item_IP &item)
    {
        if (!f_)
            return false;
        size_t w = std::fwrite(&item, 1, sizeof(Item_IP), f_);
        if (w != sizeof(Item_IP))
        {
            std::perror("fwrite");
            return false;
        }
        cursor_ += w;
        return true;
    }

    size_t get_current_offset() const
    {
        return cursor_;
    }
};

class IPDiskReader
{
public:
    IPDiskReader() : f_(nullptr) {}
    ~IPDiskReader() { close(); }
    bool open(const char *p)
    {
        close();
        f_ = std::fopen(p, "rb");
        return f_ != nullptr;
    }
    void close()
    {
        if (f_)
        {
            std::fclose(f_);
            f_ = nullptr;
        }
    }
    bool read_slice(uint64_t off, uint32_t cnt, Layer_IP &out)
    {
        if (!f_)
            return false;
        if (std::fseek(f_, (long)off, SEEK_SET) != 0)
        {
            std::perror("fseek");
            return false;
        }
        out.resize(cnt);
        size_t bytes = size_t(cnt) * sizeof(Item_IP);
        size_t r = std::fread(out.data(), 1, bytes, f_);
        if (r != bytes)
        {
            std::perror("fread");
            return false;
        }
        return true;
    }

    bool read_ip_item(uint64_t off, Item_IP &out)
    {
        if (!f_)
            return false;
        if (std::fseek(f_, (long)off, SEEK_SET) != 0)
        {
            std::perror("fseek");
            return false;
        }
        size_t r = std::fread(&out, 1, sizeof(Item_IP), f_);
        if (r != sizeof(Item_IP))
        {
            std::perror("fread");
            return false;
        }
        return true;
    }

private:
    std::FILE *f_;
};

// merge_em_ip_inplace_generic with external memory for IP storage
template <typename SrcItem, typename DstItem,
          DstItem (*merge_func)(const SrcItem &, const SrcItem &),
          void (*sort_func)(LayerVec<SrcItem> &), bool discard_zero,
          typename KeyType = uint32_t, KeyType (*key_func)(const SrcItem &) = getKey20<SrcItem>,
          const size_t move_bound = MOVE_BOUND, const size_t max_tmp_size = TMP_SIZE,
          const size_t IP_BATCH_SIZE = 65536, const size_t DELTA_SIZE = 128>
inline void merge_em_ip_inplace_generic(LayerVec<SrcItem> &src_arr,
                                        LayerVec<DstItem> &dst_arr,
                                        IPDiskWriter &ip_writer)
{
    if (src_arr.empty())
        return;
    // sanity checks
    sort_func(src_arr);
    const size_t N = src_arr.size();
    const size_t sz_src = sizeof(SrcItem), sz_dst = sizeof(DstItem);

    // Use deque as a FIFO queue for destination items
    std::vector<DstItem> tmp_items;
    std::vector<Item_IP> tmp_ips;
    std::vector<uint8_t> skip_buf;
    tmp_items.reserve(max_tmp_size);
    tmp_ips.reserve(IP_BATCH_SIZE + DELTA_SIZE);
    skip_buf.reserve(GROUP_BOUND);

    size_t free_bytes = 0;
    size_t avail_dst = dst_arr.capacity() - dst_arr.size();

    // Helper lambda to flush IP batch to disk
    auto flush_ips = [&]()
    {
        ip_writer.append_layer(tmp_ips.data(), tmp_ips.size());
        tmp_ips.resize(0); // resie or clear
    };

    size_t i = 0;
    while (i < N)
    {
        const size_t group_start = i;
        const auto key0 = key_func(src_arr[group_start]);
        i++;
        while (i < N && key_func(src_arr[i]) == key0)
            ++i;
        const size_t group_end = i;
        const size_t group_size = group_end - group_start;

        if (discard_zero)
        {
            skip_buf.assign(group_size, 0);
            for (size_t j1 = group_start; j1 < group_end; ++j1)
            {
                if (skip_buf[j1 - group_start])
                    continue;
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    if (skip_buf[j2 - group_start])
                        continue;
                    DstItem out = merge_func(src_arr[j1], src_arr[j2]);
                    if (is_zero_item(out))
                    {
                        skip_buf[j2 - group_start] = 1;
                        continue;
                    }
                    tmp_items.emplace_back(out);
                    tmp_ips.emplace_back(make_ip_pair(src_arr[j1], src_arr[j2]));
                }
            }
        }
        else
        {
            if (group_size > 3)
            {
                continue;
            } // last hop: skip large groups since they are trivial solutions with overwhelming prob.
            for (size_t j1 = group_start; j1 < group_end; ++j1)
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    tmp_items.emplace_back(merge_func(src_arr[j1], src_arr[j2]));
                    tmp_ips.emplace_back(make_ip_pair(src_arr[j1], src_arr[j2]));
                }
        }

        const size_t tmp_size = tmp_items.size();
        if (tmp_size >= avail_dst) // already full, we will throw away remaining tmp_items/tmp_ips
        {
            break;
        }
        if (tmp_ips.size() >= IP_BATCH_SIZE)
        {
            flush_ips();
        }
        free_bytes += group_size * sz_src;
        const size_t can_dst = free_bytes / sz_dst;
        const size_t to_move = std::min<size_t>({tmp_size, can_dst, avail_dst});
        if (tmp_size >= move_bound) // trade-off: to avoid too-frequent small moves, we only move when we have enough items.
        {

            // const size_t avail_dst = dst_arr.capacity() - dst_arr.size();
            drain_vectors(tmp_items, dst_arr, to_move);
            free_bytes -= to_move * sz_dst;
            avail_dst = dst_arr.capacity() - dst_arr.size();
        }
    }

    if (tmp_items.size())
    {
        // const size_t avail_dst = dst_arr.capacity() - dst_arr.size();
        const size_t to_move = std::min(tmp_items.size(), avail_dst);
        drain_vectors(tmp_items, dst_arr, to_move);
    }
    flush_ips();
}

// External memory merge wrappers
inline void merge0_em_ip_inplace(Layer0_IDX &s, Layer1_IDX &d, IPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item0_IDX, Item1_IDX, merge_item0_IDX, sort20<Item0_IDX>, true>(s, d, w);
}

inline void merge1_em_ip_inplace(Layer1_IDX &s, Layer2_IDX &d, IPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item1_IDX, Item2_IDX, merge_item1_IDX, sort20<Item1_IDX>, true>(s, d, w);
}

inline void merge2_em_ip_inplace(Layer2_IDX &s, Layer3_IDX &d, IPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item2_IDX, Item3_IDX, merge_item2_IDX, sort20<Item2_IDX>, true>(s, d, w);
}

inline void merge3_em_ip_inplace(Layer3_IDX &s, Layer4_IDX &d, IPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item3_IDX, Item4_IDX, merge_item3_IDX, sort20<Item3_IDX>, true>(s, d, w);
}

inline void merge4_em_ip_inplace(Layer4_IDX &s, Layer5_IDX &d, IPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item4_IDX, Item5_IDX, merge_item4_IDX, sort20<Item4_IDX>, true>(s, d, w);
}

inline void merge5_em_ip_inplace(Layer5_IDX &s, Layer6_IDX &d, IPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item5_IDX, Item6_IDX, merge_item5_IDX, sort20<Item5_IDX>, true>(s, d, w);
}

inline void merge6_em_ip_inplace(Layer6_IDX &s, Layer7_IDX &d, IPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item6_IDX, Item7_IDX, merge_item6_IDX, sort20<Item6_IDX>, true>(s, d, w);
}

inline void merge7_em_ip_inplace(Layer7_IDX &s, Layer8_IDX &d, IPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item7_IDX, Item8_IDX, merge_item7_IDX, sort20<Item7_IDX>, true>(s, d, w);
}

inline void merge8_em_ip_inplace(Layer8_IDX &s, Layer9_IDX &d, IPDiskWriter &w)
{
    merge_em_ip_inplace_generic<Item8_IDX, Item9_IDX, merge_item8_IDX, sort40<Item8_IDX>, false, uint64_t, getKey40<Item8_IDX>>(s, d, w);
}
