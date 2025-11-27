#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// Memory helpers
// ============================================================================

template <typename T>
struct MemAllocator
{
    using value_type = T;
    using pointer = T *;
    using size_type = std::size_t;

    T *begin_;
    size_type capacity_;
    size_type used_;

    MemAllocator() noexcept : begin_(nullptr), capacity_(0), used_(0) {}

    MemAllocator(T *b, size_type c) noexcept
        : begin_(b), capacity_(c), used_(0) {}

    template <typename U>
    MemAllocator(const MemAllocator<U> &o) noexcept
    {
        begin_ = reinterpret_cast<T *>(o.begin_);
        if (o.capacity_)
        {
            size_t bytes = o.capacity_ * sizeof(U);
            capacity_ = bytes / sizeof(T);
        }
        else
        {
            capacity_ = 0;
        }
        used_ = 0;
    }

    pointer allocate(size_type n)
    {
        if (!begin_ || used_ + n > capacity_)
        {
            throw std::bad_alloc();
        }
        pointer p = begin_ + used_;
        used_ += n;
        return p;
    }

    void deallocate(pointer, size_type) noexcept {}

    template <typename U, typename... Args>
    void construct(U *p, Args &&...args)
    {
        if constexpr (sizeof...(Args) == 0 && std::is_trivially_default_constructible_v<U>)
        {
            return;
        }
        else
        {
            ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...);
        }
    }

    template <typename U>
    void destroy(U *p)
    {
        if constexpr (!std::is_trivially_destructible_v<U>)
        {
            p->~U();
        }
    }

    size_type max_size() const noexcept { return capacity_; }

    bool operator==(const MemAllocator &r) const noexcept
    {
        return begin_ == r.begin_ && capacity_ == r.capacity_;
    }

    bool operator!=(const MemAllocator &r) const noexcept
    {
        return !(*this == r);
    }

    template <typename U>
    friend struct MemAllocator;
};

template <typename T>
using LayerVec = std::vector<T, MemAllocator<T>>;

template <typename T>
inline LayerVec<T> init_layer(uint8_t *base_ptr, size_t total_bytes)
{
    size_t cap = total_bytes / sizeof(T);
    auto alloc = MemAllocator<T>(reinterpret_cast<T *>(base_ptr), cap);
    LayerVec<T> layer(alloc);
    layer.reserve(cap);
    return layer;
}

template <typename T>
inline LayerVec<T> init_layer_with_new_memory(size_t total_bytes)
{
    size_t cap = total_bytes / sizeof(T);
    uint8_t *base_ptr = static_cast<uint8_t *>(std::malloc(total_bytes));
    if (!base_ptr)
    {
        throw std::bad_alloc();
    }
    auto alloc = MemAllocator<T>(reinterpret_cast<T *>(base_ptr), cap);
    LayerVec<T> layer(alloc);
    layer.reserve(cap);
    return layer;
}

template <typename V>
inline void clear_vec(V &v)
{
    v.resize(0);
}

// ============================================================================
// Data Structures
// ============================================================================

template <int BYTES>
struct ItemVal
{
    uint8_t XOR[BYTES];
};

template <typename T>
inline constexpr std::size_t ItemXorSize = sizeof(std::declval<T &>().XOR);

template <int BYTES, std::size_t INDEX_BYTES>
struct ItemValIdx
{
    uint8_t XOR[BYTES];
    uint8_t index[INDEX_BYTES];
};

template <std::size_t INDEX_BYTES>
struct ItemIP
{
    uint8_t index_pointer_left[INDEX_BYTES];
    uint8_t index_pointer_right[INDEX_BYTES];
};

namespace equihash
{

template <typename ItemIP>
struct IPDiskMetaT
{
    std::uint64_t offset = 0;
    std::uint64_t count = 0;
    std::uint64_t stride = sizeof(ItemIP);
};

template <typename ItemIP, std::size_t LayerCount>
struct IPDiskManifestT
{
    std::array<IPDiskMetaT<ItemIP>, LayerCount> ip;
};

} // namespace equihash

// ============================================================================
// Index helpers
// ============================================================================

template <typename ItemT>
inline void set_index(ItemT &item, size_t v)
{
    constexpr size_t kBytes = sizeof(((ItemT *)nullptr)->index);
    for (size_t i = 0; i < kBytes; ++i)
    {
        item.index[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
    }
}

template <std::size_t N>
inline size_t get_index_from_bytes(const uint8_t (&idx)[N])
{
    size_t value = 0;
    for (size_t i = 0; i < N; ++i)
    {
        value |= static_cast<size_t>(idx[i]) << (8 * i);
    }
    return value;
}

template <typename ItemT>
inline void set_index_batch(LayerVec<ItemT> &v)
{
    const size_t n = v.size();
    for (size_t i = 0; i < n; ++i)
    {
        set_index(v[i], i);
    }
}

// ============================================================================
// Layer transforms
// ============================================================================

template <typename SrcItem, typename DstItem>
inline LayerVec<DstItem> expand_layer_to_idx_inplace(LayerVec<SrcItem> &src)
{
    static_assert(sizeof(SrcItem) <= sizeof(DstItem),
                  "DstItem must be same size or larger than SrcItem");

    const size_t n = src.size();
    if (n == 0)
    {
        return LayerVec<DstItem>(MemAllocator<DstItem>());
    }

    auto alloc_src = src.get_allocator();
    uint8_t *base = reinterpret_cast<uint8_t *>(alloc_src.begin_);

    const size_t sz_src = sizeof(SrcItem);
    const size_t sz_dst = sizeof(DstItem);

    for (size_t i = n; i-- > 0;)
    {
        uint8_t *s = base + i * sz_src;
        uint8_t *d = base + i * sz_dst;

        std::memmove(d, s, sz_src);

        auto *dst_item = reinterpret_cast<DstItem *>(d);
        set_index(*dst_item, i);
    }

    MemAllocator<DstItem> alloc_dst(reinterpret_cast<DstItem *>(base), n);
    LayerVec<DstItem> result(alloc_dst);
    result.resize(n);
    src.resize(0);
    return result;
}
