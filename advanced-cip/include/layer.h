#pragma once
#include <cstddef>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <array>

#include "zcash_blake.h"

enum class SortAlgo
{
    STD,
    KXSORT
};
extern SortAlgo g_sort_algo;
extern bool g_verbose;


/**
 * @brief Custom memory allocator for pre-allocated memory regions
 *
 * This allocator manages a fixed memory region without dynamic allocation.
 * It's designed for zero-copy operations and in-place transformations.
 *
 * @tparam T Type of elements to allocate
 */
template <typename T>
struct MemAllocator
{
    using value_type = T;
    using pointer = T *;
    using size_type = std::size_t;

    T *begin_;           ///< Pointer to the start of the memory region
    size_type capacity_; ///< Maximum number of T elements that can fit
    size_type used_;     ///< Number of T elements currently allocated

    /// Default constructor for empty allocator
    MemAllocator() noexcept : begin_(nullptr), capacity_(0), used_(0) {}

    /**
     * @brief Construct allocator with pre-allocated memory
     * @param b Pointer to the beginning of memory region
     * @param c Capacity in number of T elements
     */
    MemAllocator(T *b, size_type c) noexcept
        : begin_(b), capacity_(c), used_(0) {}

    /**
     * @brief Rebind constructor for converting between allocator types
     *
     * This allows creating an allocator for type T from an allocator for type U
     * while preserving the same underlying memory region.
     *
     * @tparam U Source allocator's value type
     * @param o Source allocator to convert from
     */
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

    /**
     * @brief Allocate n elements from the pre-allocated memory region
     * @param n Number of elements to allocate
     * @return Pointer to allocated memory
     * @throws std::bad_alloc if not enough space available
     */
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

    /// Deallocation is a no-op for pre-allocated memory
    void deallocate(pointer, size_type) noexcept {}

    /**
     * @brief Construct element in-place
     *
     * For trivially default constructible types with no arguments,
     * skip construction to preserve already-prepared data.
     * This is crucial for in-place transformations like expand_layer_to_idx_inplace.
     *
     * @tparam U Element type
     * @tparam Args Constructor argument types
     * @param p Pointer to location for construction
     * @param args Constructor arguments
     */
    template <typename U, typename... Args>
    void construct(U *p, Args &&...args)
    {
        if constexpr (sizeof...(Args) == 0 && std::is_trivially_default_constructible_v<U>)
        {
            // Skip construction for trivial types with no args
            // Data is already in place from prior setup
        }
        else
        {
            ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Destroy element at given location
     *
     * For trivially destructible types, skip destruction for performance.
     *
     * @tparam U Element type
     * @param p Pointer to element to destroy
     */
    template <typename U>
    void destroy(U *p)
    {
        if constexpr (!std::is_trivially_destructible_v<U>)
        {
            p->~U();
        }
    }

    /// Return maximum possible allocation size
    size_type max_size() const noexcept { return capacity_; }

    /// Equality comparison
    bool operator==(const MemAllocator &r) const noexcept
    {
        return begin_ == r.begin_ && capacity_ == r.capacity_;
    }

    /// Inequality comparison
    bool operator!=(const MemAllocator &r) const noexcept
    {
        return !(*this == r);
    }

    template <typename U>
    friend struct MemAllocator;
};

/**
 * @brief Vector type using MemAllocator for pre-allocated memory
 * @tparam T Element type
 */
template <typename T>
using LayerVec = std::vector<T, MemAllocator<T>>;

/**
 * @brief Initialize a LayerVec with pre-allocated memory
 *
 * @tparam T Element type
 * @param base_ptr Pointer to memory region
 * @param total_bytes Total size of memory region in bytes
 * @return LayerVec configured to use the provided memory
 */
template <typename T>
inline LayerVec<T> init_layer(uint8_t *base_ptr, size_t total_bytes)
{
    size_t cap = total_bytes / sizeof(T);
    auto alloc = MemAllocator<T>(reinterpret_cast<T *>(base_ptr), cap);
    LayerVec<T> layer(alloc);
    layer.reserve(cap);
    return layer;
}

/**
 * @brief Initialize a LayerVec with newly allocated memory
 *
 * @tparam T Element type
 * @param total_bytes Size of memory to allocate in bytes
 * @return LayerVec with freshly allocated memory
 * @throws std::bad_alloc if allocation fails
 */
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

/**
 * @brief Clear a vector (resize to 0)
 * @tparam V Vector type
 * @param v Vector to clear
 */
template <typename V>
inline void clear_vec(V &v)
{
    v.resize(0);
}

// ============================================================================
// Data Structures
// ============================================================================

/// Index pointer pair for tracing solution paths
struct Item_IP
{
    uint8_t index_pointer_left[3];  ///< Left child index (24-bit)
    uint8_t index_pointer_right[3]; ///< Right child index (24-bit)
};

/**
 * @brief Item containing only XOR hash value (no index)
 * @tparam BYTES Size of the XOR field
 */
template <int BYTES>
struct ItemVal
{
    uint8_t XOR[BYTES]; ///< XOR hash value
};

// Layer items without index (for memory-efficient merging)
using Item0 = ItemVal<25>;
using Item1 = ItemVal<23>;
using Item2 = ItemVal<20>;
using Item3 = ItemVal<18>;
using Item4 = ItemVal<15>;
using Item5 = ItemVal<13>;
using Item6 = ItemVal<10>;
using Item7 = ItemVal<8>;
using Item8 = ItemVal<5>;

/**
 * @brief Item containing XOR hash value and index
 * @tparam BYTES Size of the XOR field
 */
template <int BYTES>
struct ItemValIdx
{
    uint8_t XOR[BYTES]; ///< XOR hash value
    uint8_t index[3];   ///< 24-bit index for tracing
};

// Layer items with index (for solution reconstruction)
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

// Layer type aliases
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

using Layer0 = LayerVec<Item0>;
using Layer1 = LayerVec<Item1>;
using Layer2 = LayerVec<Item2>;
using Layer3 = LayerVec<Item3>;
using Layer4 = LayerVec<Item4>;
using Layer5 = LayerVec<Item5>;
using Layer6 = LayerVec<Item6>;
using Layer7 = LayerVec<Item7>;
using Layer8 = LayerVec<Item8>;
using Layer9 = LayerVec<Item_IP>;



// ============================================================================
// Index Helpers
// ============================================================================

/**
 * @brief Set 24-bit index in an item
 * @tparam ItemT Item type with index field
 * @param item Item to set index in
 * @param v Index value to set (only lower 24 bits used)
 */
template <typename ItemT>
inline void set_index(ItemT &item, size_t v)
{
    item.index[0] = static_cast<uint8_t>(v & 0xFF);
    item.index[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    item.index[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
}

/**
 * @brief Get 24-bit index from byte array
 * @param idx 3-byte array containing index
 * @return Index value as size_t
 */
inline size_t get_index_from_bytes(const uint8_t idx[3])
{
    return static_cast<size_t>(idx[0]) |
           (static_cast<size_t>(idx[1]) << 8) |
           (static_cast<size_t>(idx[2]) << 16);
}

/**
 * @brief Load 24-bit unsigned integer from byte array
 * @param x 3-byte array
 * @return Value as uint32_t
 */
inline uint32_t load_u24(const uint8_t x[3])
{
    return static_cast<uint32_t>(x[0]) |
           (static_cast<uint32_t>(x[1]) << 8) |
           (static_cast<uint32_t>(x[2]) << 16);
}

/**
 * @brief Set sequential indices for all items in a layer
 *
 * Each item's index is set to its position in the vector.
 *
 * @tparam ItemT Item type with index field
 * @param v Vector of items
 */

template <typename ItemT>
inline void set_index_batch(LayerVec<ItemT> &v)
{
    const size_t n = v.size();
    for (size_t i = 0; i < n; ++i)
    {
        set_index(v[i], i);
    }
}

/* ===================== External-memory manifest ===================== */

struct IPDiskMeta
{
    std::uint64_t offset = 0; // byte offset in the EM file
    std::uint64_t count = 0;  // number of Item_IP records
    std::uint64_t stride = sizeof(Item_IP);
};

// We index [0..9]; usually only 2..8 are persisted to disk.
struct IPDiskManifest
{
    std::array<IPDiskMeta, 8> ip;
};

/**
 * @brief In-place expansion from ItemVal to ItemValIdx
 *
 * Transforms a layer without indices to a layer with indices by:
 * 1. Expanding each item's memory layout from back to front
 * 2. Appending index field after each XOR field
 * 3. Reinterpreting the memory as DstItem type
 *
 * This operation is zero-copy: it reuses the same memory buffer.
 *
 * @tparam SrcItem Source item type (ItemVal)
 * @tparam DstItem Destination item type (ItemValIdx)
 * @param src Source layer (will be emptied)
 * @return New layer with same data plus indices
 */
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

    // Expand from back to front to avoid overwriting unprocessed data
    for (size_t i = n; i-- > 0;)
    {
        uint8_t *s = base + i * sz_src;
        uint8_t *d = base + i * sz_dst;

        // Copy SrcItem bytes (XOR field only)
        std::memmove(d, s, sz_src);

        // Append 24-bit index immediately after XOR field
        d[sz_src + 0] = static_cast<uint8_t>(i & 0xFF);
        d[sz_src + 1] = static_cast<uint8_t>((i >> 8) & 0xFF);
        d[sz_src + 2] = static_cast<uint8_t>((i >> 16) & 0xFF);
    }

    // Create new LayerVec with DstItem view of the same memory
    MemAllocator<DstItem> alloc_dst(reinterpret_cast<DstItem *>(base), n);
    LayerVec<DstItem> result(alloc_dst);

    // resize() will call allocate() and construct()
    // Our custom construct() is a no-op for trivial types, preserving data
    result.resize(n);

    // Clear source to mark memory as moved
    src.resize(0);

    return result;
}
