#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iterator>
#include <vector>

#include "core/equihash_base.h"

// Merge tuning knobs (can be overridden per benchmark/test when needed)
#ifndef MOVE_BOUND
#define MOVE_BOUND 2048 // minimum number of items to move in one batch during in-place merge
#endif

#ifndef MAX_TMP_SIZE
#define MAX_TMP_SIZE 2500 // maximum pre-allocated temporary buffer size during in-place merge
#endif

#ifndef GROUP_BOUND
#define GROUP_BOUND 512 // maximum group size during in-place merge
#endif

// MAX_TMP_SIZE > MOVE_BOUND
// usually set MAX_TMP_SIZE slightly greater than MOVE_BOUND + GROUP_BOUND
static_assert(MAX_TMP_SIZE > MOVE_BOUND, "MAX_TMP_SIZE must be greater than MOVE_BOUND");

// ------------------ XOR shift & merge helpers ------------------
template <size_t NI, size_t NO>
inline void xor_shift_right_u8(uint8_t (&dst)[NO], const uint8_t (&a)[NI],
                               const uint8_t (&b)[NI], int shift_bits)
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
                              int shift_bits)
{
    Dst out;
    xor_shift_right_u8<sizeof(a.XOR), sizeof(out.XOR)>(out.XOR, a.XOR, b.XOR,
                                                       shift_bits);
    return out;
}


template <typename SrcWithIdx, typename ItemIP>
inline ItemIP make_ip_pair(const SrcWithIdx &a, const SrcWithIdx &b)
{
    ItemIP ip{};
    constexpr std::size_t IDX_BYTES = sizeof(a.index);
    static_assert(sizeof(a.index) == sizeof(b.index),
                  "Index buffers must be identical in size");
    static_assert(sizeof(ip.index_pointer_left) == IDX_BYTES,
                  "IP item must store the same index width");
    static_assert(sizeof(ip.index_pointer_right) == IDX_BYTES,
                  "IP item must store the same index width");
    for (std::size_t i = 0; i < IDX_BYTES; ++i)
    {
        ip.index_pointer_left[i] = a.index[i];
        ip.index_pointer_right[i] = b.index[i];
    }
    return ip;
}

template <typename ItemType, const size_t nbytes = 5>
inline bool is_zero_item(const ItemType &item) noexcept
{
    constexpr std::size_t check_len = (ItemXorSize<ItemType> < nbytes) ? ItemXorSize<ItemType> : nbytes;
    for (std::size_t i = 0; i < check_len; ++i)
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
template <typename SrcItem, typename DstItem, typename IPItem,
          DstItem (*merge_func)(const SrcItem &, const SrcItem &),
          void (*sort_func)(LayerVec<SrcItem> &), bool discard_zero,
          typename KeyType, KeyType (*key_func)(const SrcItem &),
          bool (*is_zero_func)(const DstItem &) = nullptr,
          IPItem (*make_ip_func)(const SrcItem &, const SrcItem &) = nullptr,
          bool is_last = false>
inline void merge_ip_inplace_generic(LayerVec<SrcItem> &src_arr,
                                     LayerVec<DstItem> &dst_arr,
                                     LayerVec<IPItem> &ip_arr)
{
    static_assert(key_func != nullptr, "Key extractor must be provided");
    static_assert(!discard_zero || is_zero_func != nullptr,
                  "Zero-check function required when discarding zeros");
    static_assert(make_ip_func != nullptr,
                  "IP construction callback must be provided");
    if (src_arr.empty())
        return;
    // assert(dst_arr.capacity() > 0 && ip_arr.capacity() > 0);
    // assert(dst_arr.size() == 0 && ip_arr.size() == 0);
    // assert(dst_arr.capacity() == ip_arr.capacity());
    sort_func(src_arr);
    const size_t N = src_arr.size();
    // const size_t sz_src = sizeof(SrcItem), sz_dst = std::max(sizeof(DstItem), sizeof(IPItem));
    const size_t sz_src = sizeof(SrcItem), sz_dst = sizeof(DstItem);

    // We can also use a FIFO queue so that items are emitted in the same order
    std::vector<DstItem> tmp_items;
    std::vector<IPItem> tmp_ips;
    std::vector<uint8_t> skip_buf;
    tmp_items.reserve(MAX_TMP_SIZE);
    tmp_ips.reserve(MAX_TMP_SIZE);
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
                    if (is_zero_func(out))
                    {
                        skip_buf[j2 - group_start] = 1;
                        continue;
                    }
                    tmp_items.emplace_back(out);
                    tmp_ips.emplace_back(make_ip_func(src_arr[j1], src_arr[j2]));
                }
            }
        }
        else
        {
            if (is_last && group_size > 3)
            {
                continue;
            } // last hop: skip large groups only for final merge
            for (size_t j1 = group_start; j1 < group_end; ++j1)
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    tmp_items.emplace_back(merge_func(src_arr[j1], src_arr[j2]));
                    tmp_ips.emplace_back(make_ip_func(src_arr[j1], src_arr[j2]));
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
        if (to_move >= MOVE_BOUND) // trade-off: to avoid too-frequent small moves, we only move when we have enough items.
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

// ------------------ Merge (in-place) without IP capture ------------------
// `src_arr` and `dst_arr` share the same memory region for memory-efficiency.
template <typename SrcItem, typename DstItem,
          DstItem (*merge_func)(const SrcItem &, const SrcItem &),
          void (*sort_func)(LayerVec<SrcItem> &), bool discard_zero,
          typename KeyType, KeyType (*key_func)(const SrcItem &),
          bool (*is_zero_func)(const DstItem &) = nullptr,
          bool is_last = false>
inline void merge_inplace_generic(LayerVec<SrcItem> &src_arr,
                                  LayerVec<DstItem> &dst_arr)
{
    static_assert(key_func != nullptr, "Key extractor must be provided");
    static_assert(!discard_zero || is_zero_func != nullptr,
                  "Zero-check function required when discarding zeros");
    if (src_arr.empty())
        return;
    sort_func(src_arr);
    const size_t N = src_arr.size();
    // const size_t sz_src = sizeof(SrcItem), sz_dst = std::max(sizeof(DstItem), sizeof(IPItem));
    const size_t sz_src = sizeof(SrcItem), sz_dst = sizeof(DstItem);

    std::vector<DstItem> tmp_items;
    std::vector<uint8_t> skip_buf;
    tmp_items.reserve(MAX_TMP_SIZE);
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
                    if (is_zero_func(out))
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
            if (is_last && group_size > 3)
            {
                continue;
            } // last hop: skip large groups only for final merge
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
        if (to_move >= MOVE_BOUND) // trade-off: to avoid too-frequent small moves, we only move when we have enough items.
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

// ------------------ Merge (in-place): output only IP ------------------
// `src_arr` and `dst_arr` share the same memory region for memory-efficiency.
template <typename SrcItem, typename DstItem, typename IPItem,
          DstItem (*merge_func)(const SrcItem &, const SrcItem &),
          void (*sort_func)(LayerVec<SrcItem> &), bool discard_zero,
          typename KeyType, KeyType (*key_func)(const SrcItem &),
          bool (*is_zero_func)(const DstItem &) = nullptr,
          IPItem (*make_ip_func)(const SrcItem &, const SrcItem &) = nullptr,
          bool is_last = false>
inline void merge_inplace_for_ip_generic(LayerVec<SrcItem> &src_arr,
                                         LayerVec<IPItem> &dst_arr)
{
    static_assert(key_func != nullptr, "Key extractor must be provided");
    static_assert(!discard_zero || is_zero_func != nullptr,
                  "Zero-check function required when discarding zeros");
    static_assert(make_ip_func != nullptr,
                  "IP construction callback must be provided");
    if (src_arr.empty())
        return;
    sort_func(src_arr);
    const size_t N = src_arr.size();
    // const size_t sz_src = sizeof(SrcItem), sz_dst = std::max(sizeof(DstItem), sizeof(IPItem));
    const size_t sz_src = sizeof(SrcItem), sz_dst = sizeof(IPItem);

    // FIFO buffer for temporary IP items
    std::vector<IPItem> tmp_items;
    std::vector<uint8_t> skip_buf;
    tmp_items.reserve(MAX_TMP_SIZE);
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
                    if (is_zero_func(out))
                    {
                        skip_buf[j2 - group_start] = 1;
                        continue;
                    }
                    tmp_items.emplace_back(make_ip_func(src_arr[j1], src_arr[j2]));
                }
            }
        }
        else
        {
            if (is_last && group_size > 3)
            {
                continue;
            } // last hop: skip large groups only for final merge
            for (size_t j1 = group_start; j1 < group_end; ++j1)
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    tmp_items.emplace_back(make_ip_func(src_arr[j1], src_arr[j2]));
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
        if (to_move >= MOVE_BOUND) // trade-off: to avoid too-frequent small moves, we only move when we have enough items.
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

// external memory classes

template <typename IPItem>
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

    uint64_t append_layer(const IPItem *data, size_t n)
    {
        if (!f_ || !n)
            return cursor_;
        size_t bytes = n * sizeof(IPItem);
        size_t w = std::fwrite(data, 1, bytes, f_);
        if (w != bytes)
            std::perror("fwrite");
        uint64_t off = cursor_;
        cursor_ += w;
        return off;
    }

    bool write_ip_item(const IPItem &item)
    {
        if (!f_)
            return false;
        size_t w = std::fwrite(&item, 1, sizeof(IPItem), f_);
        if (w != sizeof(IPItem))
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

template <typename IPItem>
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
    bool read_slice(uint64_t off, uint32_t cnt, LayerVec<IPItem> &out)
    {
        if (!f_)
            return false;
        if (std::fseek(f_, (long)off, SEEK_SET) != 0)
        {
            std::perror("fseek");
            return false;
        }
        out.resize(cnt);
        size_t bytes = size_t(cnt) * sizeof(IPItem);
        size_t r = std::fread(out.data(), 1, bytes, f_);
        if (r != bytes)
        {
            std::perror("fread");
            return false;
        }
        return true;
    }

    bool read_ip_item(uint64_t off, IPItem &out)
    {
        if (!f_)
            return false;
        if (std::fseek(f_, (long)off, SEEK_SET) != 0)
        {
            std::perror("fseek");
            return false;
        }
        size_t r = std::fread(&out, 1, sizeof(IPItem), f_);
        if (r != sizeof(IPItem))
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
template <typename SrcItem, typename DstItem, typename IPItem,
          DstItem (*merge_func)(const SrcItem &, const SrcItem &),
          void (*sort_func)(LayerVec<SrcItem> &), bool discard_zero,
          typename KeyType, KeyType (*key_func)(const SrcItem &),
          bool (*is_zero_func)(const DstItem &) = nullptr,
          IPItem (*make_ip_func)(const SrcItem &, const SrcItem &) = nullptr,
          const size_t IP_BATCH_SIZE = 65536, const size_t DELTA_SIZE = 128,
          bool is_last = false>
inline void merge_em_ip_inplace_generic(LayerVec<SrcItem> &src_arr,
                                        LayerVec<DstItem> &dst_arr,
                                        IPDiskWriter<IPItem> &ip_writer)
{
    static_assert(key_func != nullptr, "Key extractor must be provided");
    static_assert(!discard_zero || is_zero_func != nullptr,
                  "Zero-check function required when discarding zeros");
    static_assert(make_ip_func != nullptr,
                  "IP construction callback must be provided");
    if (src_arr.empty())
        return;
    // sanity checks
    sort_func(src_arr);
    const size_t N = src_arr.size();
    const size_t sz_src = sizeof(SrcItem), sz_dst = sizeof(DstItem);

    // Use deque as a FIFO queue for destination items
    std::vector<DstItem> tmp_items;
    std::vector<IPItem> tmp_ips;
    std::vector<uint8_t> skip_buf;
    tmp_items.reserve(MAX_TMP_SIZE);
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
                    if (is_zero_func(out))
                    {
                        skip_buf[j2 - group_start] = 1;
                        continue;
                    }
                    tmp_items.emplace_back(out);
                    tmp_ips.emplace_back(make_ip_func(src_arr[j1], src_arr[j2]));
                }
            }
        }
        else
        {
            if (is_last && group_size > 3)
            {
                continue;
            } // last hop: skip large groups only for final merge
            for (size_t j1 = group_start; j1 < group_end; ++j1)
                for (size_t j2 = j1 + 1; j2 < group_end; ++j2)
                {
                    tmp_items.emplace_back(merge_func(src_arr[j1], src_arr[j2]));
                    tmp_ips.emplace_back(make_ip_func(src_arr[j1], src_arr[j2]));
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
        if (tmp_size >= MOVE_BOUND) // trade-off: to avoid too-frequent small moves, we only move when we have enough items.
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


// merge_iv_inplace_generic: Layer_IV_{i} ==> Layer_IV_{i+1}
// SrcIV and DstIV must be IndexVector type: DstIV stores merged indices from two SrcIVs (IV_{i} to IV_{i+1})
// src_arr and dst_arr do not share memory, thus you no longer need to worry about memory overlap.
// just write the newly merged items to dst_arr, no tmp buffer and free_bytes management needed.
//
// Template parameters:
// - SrcIV: IndexVector<INDEX_BYTES, LAYER> for source layer
// - DstIV: IndexVector<INDEX_BYTES, LAYER+1> for destination layer
// - merge_iv_func: function to merge two SrcIVs into one DstIV (use merge_iv from equihash_base.h)
// - sort_iv_func: function to sort src_arr by key with seed (uses seed-dependent hash computation)
// - discard_zero: whether to discard zero XOR results (false for Equihash 144,5)
// - KeyType: type of the collision key (e.g., uint32_t for 24-bit keys)
// - key_func: function to extract key from SrcIV with seed (uses seed-dependent hash computation)
// - is_zero_func: optional function to check if merged result is zero with seed (for discard_zero=true)
// - is_last: whether this is the final merge layer (skip large groups)
template <typename SrcIV, typename DstIV,
          DstIV (*merge_iv_func)(const SrcIV &, const SrcIV &),
          void (*sort_iv_func)(LayerVec<SrcIV> &, int), bool discard_zero,
          typename KeyType, KeyType (*key_func)(int, const SrcIV &),
          bool (*is_zero_func)(int, const DstIV &) = nullptr,
          bool is_last = false>
inline void merge_iv_inplace_generic(LayerVec<SrcIV> &src_arr, LayerVec<DstIV> &dst_arr, int seed)
{
    static_assert(key_func != nullptr, "Key extractor must be provided");
    static_assert(!discard_zero || is_zero_func != nullptr,
                  "Zero-check function required when discarding zeros");

    if (src_arr.empty())
        return;

    // Sort source array by collision key (seed-dependent)
    sort_iv_func(src_arr, seed);

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
        const auto key0 = key_func(seed, src_arr[group_start]);
        i++;
        while (i < N && key_func(seed, src_arr[i]) == key0)
            ++i;
        const size_t group_end = i;
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