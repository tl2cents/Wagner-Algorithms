#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "kxsort.h"
#include "zcash_blake.h"

// ------------------- Tunables -------------------
#define MAX_LIST_SIZE 2100000
#define MAX_IP_LIST_SIZE 2200000
#define INITIAL_LIST_SIZE 2097152
#define MAX_TMP_ARR_SIZE 4096

static constexpr int N_BITS = 200;
static constexpr int K_ROUNDS = 9;
static constexpr int ELL_BITS = 20;

enum class SortAlgo { STD, KXSORT };
extern SortAlgo g_sort_algo;
extern bool g_verbose;

// Forward declare helper function
inline size_t get_index_from_bytes(const uint8_t idx[3]);

// --------------- Debug helpers ---------------
template <typename ItemT>
inline void print_item_hash(const char* label, const ItemT& item,
                            size_t idx = 0) {
    if (!g_verbose) return;
    std::printf("%s[%zu]: ", label, idx);
    for (size_t i = 0; i < sizeof(item.XOR); ++i) {
        std::printf("%02x", item.XOR[i]);
    }
    std::printf("\n");
}

template <typename LayerT>
inline void print_layer_debug(const char* name, const LayerT& layer,
                              size_t show_count = 3) {
    if (!g_verbose) return;
    std::printf("[DEBUG] %s: size=%zu\n", name, layer.size());
    size_t n = std::min(show_count, layer.size());
    for (size_t i = 0; i < n; ++i) {
        std::printf("  [%zu]: ", i);
        for (size_t j = 0; j < sizeof(layer[i].XOR); ++j) {
            std::printf("%02x", layer[i].XOR[j]);
        }
        // Check if item has index field (ItemValIdx types do, ItemVal types
        // don't)
        if constexpr (sizeof(layer[i]) > sizeof(layer[i].XOR)) {
            std::printf(" idx=%zu", get_index_from_bytes(layer[i].index));
        }
        std::printf("\n");
    }
}

inline void print_solution_indices(const char* label,
                                   const std::vector<size_t>& indices) {
    if (!g_verbose) return;
    std::printf("%s (%zu indices): ", label, indices.size());
    size_t show_n = std::min<size_t>(16, indices.size());
    for (size_t i = 0; i < show_n; ++i) {
        std::printf("%zx ", indices[i]);
    }
    if (indices.size() > show_n) std::printf("...");
    std::printf("\n");
}

// --------------- MemAllocator & Layer ---------------
template <typename T>
struct MemAllocator {
    using value_type = T;
    using pointer = T*;
    using size_type = std::size_t;
    T* begin_;
    size_type capacity_;
    size_type used_;
    MemAllocator() noexcept : begin_(nullptr), capacity_(0), used_(0) {}
    MemAllocator(T* b, size_type c) noexcept
        : begin_(b), capacity_(c), used_(0) {}
    template <typename U>
    MemAllocator(const MemAllocator<U>& o) noexcept {
        begin_ = reinterpret_cast<T*>(o.begin_);
        if (o.capacity_) {
            size_t bytes = o.capacity_ * sizeof(U);
            capacity_ = bytes / sizeof(T);
        } else
            capacity_ = 0;
        used_ = 0;
    }
    pointer allocate(size_type n) {
        if (!begin_ || used_ + n > capacity_) throw std::bad_alloc();
        pointer p = begin_ + used_;
        used_ += n;
        return p;
    }
    void deallocate(pointer, size_type) noexcept {}
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new ((void*)p) U(std::forward<Args>(args)...);
    }
    template <typename U>
    void destroy(U* p) {
        p->~U();
    }
    size_type max_size() const noexcept { return capacity_; }
    bool operator==(const MemAllocator& r) const noexcept {
        return begin_ == r.begin_ && capacity_ == r.capacity_;
    }
    bool operator!=(const MemAllocator& r) const noexcept {
        return !(*this == r);
    }
    template <typename U>
    friend struct MemAllocator;
};

template <typename T>
using LayerVec = std::vector<T, MemAllocator<T>>;

// --------------- Data structures ---------------
struct Item_IP {
    uint8_t index_pointer_left[3];
    uint8_t index_pointer_right[3];
};

template <int BYTES>
struct ItemVal {
    uint8_t XOR[BYTES];
};
using Item0 = ItemVal<25>;
using Item1 = ItemVal<23>;
using Item2 = ItemVal<20>;
using Item3 = ItemVal<18>;
using Item4 = ItemVal<15>;
using Item5 = ItemVal<13>;
using Item6 = ItemVal<10>;
using Item7 = ItemVal<8>;
using Item8 = ItemVal<5>;
using Item9 = ItemVal<3>;

template <int BYTES>
struct ItemValIdx {
    uint8_t XOR[BYTES];
    uint8_t index[3];
};
using Item0_IDX = ItemValIdx<25>;
using Item1_IDX = ItemValIdx<23>;
using Item2_IDX = ItemValIdx<20>;
using Item3_IDX = ItemValIdx<18>;
using Item4_IDX = ItemValIdx<15>;
using Item5_IDX = ItemValIdx<13>;
using Item6_IDX = ItemValIdx<10>;
using Item7_IDX = ItemValIdx<8>;
using Item8_IDX = ItemValIdx<5>;
using Item9_IDX = ItemValIdx<3>;

using Layer_IP = LayerVec<Item_IP>;
using Layer0_IDX = LayerVec<Item0_IDX>;
using Layer1_IDX = LayerVec<Item1_IDX>;
using Layer2_IDX = LayerVec<Item2_IDX>;
using Layer3_IDX = LayerVec<Item3_IDX>;
using Layer4_IDX = LayerVec<Item4_IDX>;
using Layer5_IDX = LayerVec<Item5_IDX>;
using Layer6_IDX = LayerVec<Item6_IDX>;
using Layer7_IDX = LayerVec<Item7_IDX>;
using Layer8_IDX = LayerVec<Item8_IDX>;
using Layer9_IDX = LayerVec<Item9_IDX>;

static inline uint64_t MAX_MEM_BYTES = MAX_LIST_SIZE * sizeof(Item0_IDX);
static inline uint64_t MAX_IP_MEM_BYTES = MAX_IP_LIST_SIZE * sizeof(Item_IP);

// --------------- Layer init ---------------
template <typename T>
inline LayerVec<T> init_Layer(uint8_t* base_ptr, size_t total_bytes) {
    size_t cap = total_bytes / sizeof(T);
    auto alloc = MemAllocator<T>(reinterpret_cast<T*>(base_ptr), cap);
    LayerVec<T> layer(alloc);
    layer.reserve(cap);
    return layer;
}
template <typename T>
inline LayerVec<T> init_slice(uint8_t* base_ptr, size_t bytes_slice) {
    return init_Layer<T>(base_ptr, bytes_slice);
}

// --------------- Index helpers ---------------
template <typename ItemT>
inline void set_index(ItemT& item, size_t v) {
    item.index[0] = uint8_t(v & 0xFF);
    item.index[1] = uint8_t((v >> 8) & 0xFF);
    item.index[2] = uint8_t((v >> 16) & 0xFF);
}
inline size_t get_index_from_bytes(const uint8_t idx[3]) {
    return size_t(idx[0]) | (size_t(idx[1]) << 8) | (size_t(idx[2]) << 16);
}
template <typename ItemT>
inline void set_index_batch(LayerVec<ItemT>& v) {
    for (size_t i = 0; i < v.size(); ++i) set_index(v[i], i);
}

// --------------- Keys & sort ---------------
template <typename T>
inline uint32_t getKey20(const T& x) {
    uint64_t t = 0;
    std::memcpy(&t, x.XOR, std::min(sizeof(x.XOR), sizeof(t)));
    return uint32_t(t & 0x000FFFFFul);
}
template <typename T>
inline uint64_t getKey40(const T& x) {
    uint64_t t = 0;
    std::memcpy(&t, x.XOR, std::min(sizeof(x.XOR), sizeof(t)));
    return (t & 0xFFFFFFFFFFull);
}

template <typename T>
inline void std_sort20(LayerVec<T>& a) {
    std::sort(a.begin(), a.end(),
              [](auto& x, auto& y) { return getKey20(x) < getKey20(y); });
}
template <typename T>
inline void std_sort40(LayerVec<T>& a) {
    std::sort(a.begin(), a.end(),
              [](auto& x, auto& y) { return getKey40(x) < getKey40(y); });
}

template <typename T>
struct Radix20Traits {
    static const int nBytes = 3;
    inline uint32_t kth_byte(const T& x, int k) const {
        auto v = getKey20(x);
        return (v >> (8 * k)) & 0xFFu;
    }
    inline bool compare(const T& a, const T& b) const {
        return getKey20(a) < getKey20(b);
    }
};
template <typename T>
struct Radix40Traits {
    static const int nBytes = 5;
    inline uint32_t kth_byte(const T& x, int k) const {
        auto v = getKey40(x);
        return (uint32_t)((v >> (8 * k)) & 0xFFu);
    }
    inline bool compare(const T& a, const T& b) const {
        return getKey40(a) < getKey40(b);
    }
};
template <typename T>
inline void kx_sort20(LayerVec<T>& a) {
    kx::radix_sort(a.begin(), a.end(), Radix20Traits<T>{});
}
template <typename T>
inline void kx_sort40(LayerVec<T>& a) {
    kx::radix_sort(a.begin(), a.end(), Radix40Traits<T>{});
}

template <typename T>
inline void sort20(LayerVec<T>& a) {
    (g_sort_algo == SortAlgo::STD) ? std_sort20(a) : kx_sort20(a);
}
template <typename T>
inline void sort40(LayerVec<T>& a) {
    (g_sort_algo == SortAlgo::STD) ? std_sort40(a) : kx_sort40(a);
}

// --------------- XOR shift kernel ---------------
template <size_t NI, size_t NO>
inline void xor_shift_right_u8(uint8_t (&dst)[NO], const uint8_t (&a)[NI],
                               const uint8_t (&b)[NI],
                               int shift_bits = ELL_BITS) {
    uint8_t tmp[NI];
    for (size_t i = 0; i < NI; ++i) tmp[i] = a[i] ^ b[i];
    const int by = shift_bits / 8, bt = shift_bits % 8;
    const uint8_t* s = tmp + by;

    // Handle all bytes except potentially the last one
    for (size_t i = 0; i + 1 < NO; ++i) {
        dst[i] = uint8_t((s[i] >> bt) | (s[i + 1] << (8 - bt)));
    }

    // Handle the last byte: check if there's a next byte to read from
    if (by + NO < NI) {
        // There's still data available, combine with next byte
        dst[NO - 1] = uint8_t((s[NO - 1] >> bt) | (s[NO] << (8 - bt)));
    } else {
        // No more data, just shift the last byte
        dst[NO - 1] = uint8_t(s[NO - 1] >> bt);
    }
}
template <typename Src, typename Dst>
inline Dst merge_item_generic(const Src& a, const Src& b,
                              int shift_bits = ELL_BITS) {
    Dst out;
    xor_shift_right_u8<sizeof(a.XOR), sizeof(out.XOR)>(out.XOR, a.XOR, b.XOR,
                                                       shift_bits);
    return out;
}

inline Item1_IDX merge_item0_IDX(const Item0_IDX& a, const Item0_IDX& b) {
    return merge_item_generic<Item0_IDX, Item1_IDX>(a, b);
}
inline Item2_IDX merge_item1_IDX(const Item1_IDX& a, const Item1_IDX& b) {
    return merge_item_generic<Item1_IDX, Item2_IDX>(a, b);
}
inline Item3_IDX merge_item2_IDX(const Item2_IDX& a, const Item2_IDX& b) {
    return merge_item_generic<Item2_IDX, Item3_IDX>(a, b);
}
inline Item4_IDX merge_item3_IDX(const Item3_IDX& a, const Item3_IDX& b) {
    return merge_item_generic<Item3_IDX, Item4_IDX>(a, b);
}
inline Item5_IDX merge_item4_IDX(const Item4_IDX& a, const Item4_IDX& b) {
    return merge_item_generic<Item4_IDX, Item5_IDX>(a, b);
}
inline Item6_IDX merge_item5_IDX(const Item5_IDX& a, const Item5_IDX& b) {
    return merge_item_generic<Item5_IDX, Item6_IDX>(a, b);
}
inline Item7_IDX merge_item6_IDX(const Item6_IDX& a, const Item6_IDX& b) {
    return merge_item_generic<Item6_IDX, Item7_IDX>(a, b);
}
inline Item8_IDX merge_item7_IDX(const Item7_IDX& a, const Item7_IDX& b) {
    return merge_item_generic<Item7_IDX, Item8_IDX>(a, b);
}
inline Item9_IDX merge_item8_IDX(const Item8_IDX& a, const Item8_IDX& b) {
    return merge_item_generic<Item8_IDX, Item9_IDX>(a, b);
}

template <typename SrcWithIdx>
inline Item_IP make_ip_pair(const SrcWithIdx& a, const SrcWithIdx& b) {
    Item_IP ip{};
    for (int i = 0; i < 3; ++i) {
        ip.index_pointer_left[i] = a.index[i];
        ip.index_pointer_right[i] = b.index[i];
    }
    return ip;
}

// --------------- Merge (in-place) with IP capture ---------------
template <typename SrcItem, typename DstItem,
          DstItem (*merge_func)(const SrcItem&, const SrcItem&),
          void (*sort_func)(LayerVec<SrcItem>&), bool discard_zero,
          typename KeyType = uint32_t,
          KeyType (*key_func)(const SrcItem&) = getKey20<SrcItem>>
inline void merge_ip_inplace_generic(LayerVec<SrcItem>& src_arr,
                                     LayerVec<DstItem>& dst_arr,
                                     Layer_IP& ip_arr) {
    if (src_arr.empty()) return;
    sort_func(src_arr);

    const size_t N = src_arr.size();
    const uint32_t sz_src = sizeof(SrcItem), sz_dst = sizeof(DstItem),
                   sz_ip = sizeof(Item_IP);
    uint32_t free_bytes = 0;

    std::vector<DstItem> tmp_items;
    tmp_items.reserve(MAX_TMP_ARR_SIZE);
    std::vector<Item_IP> tmp_ips;
    tmp_ips.reserve(MAX_TMP_ARR_SIZE);

    size_t i = 0;
    while (i < N) {
        const size_t g0 = i;
        const auto key0 = key_func(src_arr[g0]);
        while (i < N && key_func(src_arr[i]) == key0) ++i;
        const size_t g1 = i;
        const size_t gsize = g1 - g0;

        if (discard_zero) {
            std::vector<char> skip(gsize, 0);
            for (size_t a = g0; a < g1; ++a) {
                if (skip[a - g0]) continue;
                for (size_t b = a + 1; b < g1; ++b) {
                    if (skip[b - g0]) continue;
                    DstItem out = merge_func(src_arr[a], src_arr[b]);
                    bool zero40 = true;
                    for (int t = 0; t < std::min<int>(5, (int)sizeof(out.XOR));
                         ++t) {
                        if (out.XOR[t]) {
                            zero40 = false;
                            break;
                        }
                    }
                    if (zero40) {
                        skip[b - g0] = 1;
                        continue;
                    }
                    tmp_items.push_back(out);
                    tmp_ips.push_back(make_ip_pair(src_arr[a], src_arr[b]));
                }
            }
        } else {
            if (gsize > 3) {
                continue;
            }  // last hop: skip large groups
            for (size_t a = g0; a < g1; ++a)
                for (size_t b = a + 1; b < g1; ++b) {
                    tmp_items.push_back(merge_func(src_arr[a], src_arr[b]));
                    tmp_ips.push_back(make_ip_pair(src_arr[a], src_arr[b]));
                }
        }

        free_bytes += uint32_t(gsize * sz_src);
        const uint32_t can_dst = free_bytes / sz_dst;
        const uint32_t can_ip = free_bytes / sz_ip;
        const size_t tmp_cnt = std::min(tmp_items.size(), tmp_ips.size());
        if (tmp_cnt) {
            const size_t avail_dst = dst_arr.capacity() - dst_arr.size();
            const size_t avail_ip = ip_arr.capacity() - ip_arr.size();
            const size_t to_move =
                std::min<size_t>({tmp_cnt, (size_t)can_dst, (size_t)can_ip,
                                  avail_dst, avail_ip});
            if (to_move) {
                const size_t od = dst_arr.size();
                dst_arr.resize(od + to_move);
                std::move(tmp_items.end() - to_move, tmp_items.end(),
                          dst_arr.begin() + od);
                tmp_items.resize(tmp_items.size() - to_move);
                const size_t oi = ip_arr.size();
                ip_arr.resize(oi + to_move);
                std::move(tmp_ips.end() - to_move, tmp_ips.end(),
                          ip_arr.begin() + oi);
                tmp_ips.resize(tmp_ips.size() - to_move);
                free_bytes -= uint32_t(to_move * std::max(sz_dst, sz_ip));
            }
        }
    }

    const size_t tmp_cnt = std::min(tmp_items.size(), tmp_ips.size());
    if (tmp_cnt) {
        const size_t avail_dst = dst_arr.capacity() - dst_arr.size();
        const size_t avail_ip = ip_arr.capacity() - ip_arr.size();
        const size_t to_move = std::min(tmp_cnt, std::min(avail_dst, avail_ip));
        if (to_move) {
            const size_t od = dst_arr.size();
            dst_arr.resize(od + to_move);
            std::move(tmp_items.end() - to_move, tmp_items.end(),
                      dst_arr.begin() + od);
            tmp_items.resize(tmp_items.size() - to_move);
            const size_t oi = ip_arr.size();
            ip_arr.resize(oi + to_move);
            std::move(tmp_ips.end() - to_move, tmp_ips.end(),
                      ip_arr.begin() + oi);
            tmp_ips.resize(tmp_ips.size() - to_move);
        }
    }
}

inline void merge0_ip_inplace(Layer0_IDX& s, Layer1_IDX& d, Layer_IP& ip) {
    merge_ip_inplace_generic<Item0_IDX, Item1_IDX, merge_item0_IDX,
                             sort20<Item0_IDX>, true>(s, d, ip);
}
inline void merge1_ip_inplace(Layer1_IDX& s, Layer2_IDX& d, Layer_IP& ip) {
    merge_ip_inplace_generic<Item1_IDX, Item2_IDX, merge_item1_IDX,
                             sort20<Item1_IDX>, true>(s, d, ip);
}
inline void merge2_ip_inplace(Layer2_IDX& s, Layer3_IDX& d, Layer_IP& ip) {
    merge_ip_inplace_generic<Item2_IDX, Item3_IDX, merge_item2_IDX,
                             sort20<Item2_IDX>, true>(s, d, ip);
}
inline void merge3_ip_inplace(Layer3_IDX& s, Layer4_IDX& d, Layer_IP& ip) {
    merge_ip_inplace_generic<Item3_IDX, Item4_IDX, merge_item3_IDX,
                             sort20<Item3_IDX>, true>(s, d, ip);
}
inline void merge4_ip_inplace(Layer4_IDX& s, Layer5_IDX& d, Layer_IP& ip) {
    merge_ip_inplace_generic<Item4_IDX, Item5_IDX, merge_item4_IDX,
                             sort20<Item4_IDX>, true>(s, d, ip);
}
inline void merge5_ip_inplace(Layer5_IDX& s, Layer6_IDX& d, Layer_IP& ip) {
    merge_ip_inplace_generic<Item5_IDX, Item6_IDX, merge_item5_IDX,
                             sort20<Item5_IDX>, true>(s, d, ip);
}
inline void merge6_ip_inplace(Layer6_IDX& s, Layer7_IDX& d, Layer_IP& ip) {
    merge_ip_inplace_generic<Item6_IDX, Item7_IDX, merge_item6_IDX,
                             sort20<Item6_IDX>, true>(s, d, ip);
}
inline void merge7_ip_inplace(Layer7_IDX& s, Layer8_IDX& d, Layer_IP& ip) {
    merge_ip_inplace_generic<Item7_IDX, Item8_IDX, merge_item7_IDX,
                             sort20<Item7_IDX>, true>(s, d, ip);
}
inline void merge8_ip_inplace(Layer8_IDX& s, Layer9_IDX& d, Layer_IP& ip) {
    merge_ip_inplace_generic<Item8_IDX, Item9_IDX, merge_item8_IDX,
                             sort40<Item8_IDX>, false, uint64_t,
                             getKey40<Item8_IDX>>(s, d, ip);
}

/* ===================== External-memory manifest ===================== */

struct IPDiskMeta {
    std::uint64_t offset = 0;  // byte offset in the EM file
    std::uint32_t count = 0;   // number of Item_IP records
    std::uint32_t stride = sizeof(Item_IP);
};

// We index [0..9]; usually only 2..8 are persisted to disk.
struct IPDiskManifest {
    std::array<IPDiskMeta, 10> ip;

    // synthesize a deterministic header/nonce from 'seed' (so CLI兼容)
    inline void fill_layer0_from_seed(Layer0_IDX& L0, int seed) {
        uint8_t header[140];
        std::memset(header, 0, sizeof(header));
        header[0] = uint8_t(seed & 0xFF);
        header[1] = uint8_t((seed >> 8) & 0xFF);
        header[2] = uint8_t((seed >> 16) & 0xFF);
        header[3] = uint8_t((seed >> 24) & 0xFF);
        uint8_t nonce[32];
        std::memset(nonce, 0, sizeof(nonce));
        nonce[0] = header[0];
        nonce[1] = header[1];
        nonce[2] = header[2];
        nonce[3] = header[3];

        // allocate L0
        static constexpr uint32_t HALF = 1u << 20;  // 1,048,576
        static constexpr uint32_t FULL = HALF * 2;  // 2,097,152
        L0.resize(FULL);

        // Build midstate once
        ZcashEquihashHasher H;
        H.init_midstate(header, sizeof(header), nonce, /*n=*/200, /*k=*/9);

        // Process indices in blocks of 4 (NBLAKES=4)
        uint8_t out[4][ZcashEquihashHasher::OUT_LEN];
        uint32_t i = 0;
        for (; i + 3 < HALF; i += 4) {
            H.hash_index(i + 0, out[0]);
            H.hash_index(i + 1, out[1]);
            H.hash_index(i + 2, out[2]);
            H.hash_index(i + 3, out[3]);

            // Store 8 leaves (2 per index)
            for (int lane = 0; lane < 4; ++lane) {
                const int base = (2 * (i + lane));
                std::memcpy(L0[base + 0].XOR, out[lane] + 0, 25);
                std::memcpy(L0[base + 1].XOR, out[lane] + 25, 25);
            }
        }
        // tail
        for (; i < HALF; ++i) {
            H.hash_index(i, out[0]);
            std::memcpy(L0[2 * i + 0].XOR, out[0] + 0, 25);
            std::memcpy(L0[2 * i + 1].XOR, out[0] + 25, 25);
        }
        set_index_batch(L0);
    }
};

// --------------- Helpers ---------------
template <typename V>
inline void clear_vec(V& v) {
    v.resize(0);
}
static inline uint32_t load_u24(const uint8_t x[3]) {
    return (uint32_t)x[0] | ((uint32_t)x[1] << 8) | ((uint32_t)x[2] << 16);
}
