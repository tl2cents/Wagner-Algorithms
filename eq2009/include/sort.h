#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "layer.h"
#include "kxsort.h"

enum class SortAlgo
{
    STD,
    KXSORT
};
extern SortAlgo g_sort_algo;
extern bool g_verbose;
inline constexpr uint32_t K20_MASK = 0x000FFFFFu;
inline constexpr uint64_t K40_MASK = 0xFFFFFFFFFFULL;

// ============================================================================
// Key Extraction Functions
// ============================================================================

/**
 * @brief Extract lower 20 bits from item's XOR field as sort key
 * 
 * Reads up to 8 bytes from XOR field and returns lower 20 bits.
 * Used for sorting in rounds 0-7.
 * 
 * @tparam T Item type
 * @param x Item to extract key from
 * @return 20-bit key (in uint32_t)
 */
template <typename T>
inline uint32_t getKey20(const T& x) {
    uint32_t t = 0;
    // std::memcpy(&t, x.XOR, std::min(sizeof(x.XOR), sizeof(t)));
    std::memcpy(&t, x.XOR, 4); // Read 4 bytes for efficiency
    return t & K20_MASK;
}

/**
 * @brief Extract lower 40 bits from item's XOR field as sort key
 * 
 * Reads up to 8 bytes from XOR field and returns lower 40 bits.
 * Used for sorting in round 8 (final round).
 * 
 * @tparam T Item type
 * @param x Item to extract key from
 * @return 40-bit key (in uint64_t)
 */
template <typename T>
inline uint64_t getKey40(const T& x) {
    uint64_t t = 0;
    std::memcpy(&t, x.XOR, std::min(sizeof(x.XOR), sizeof(t)));
    return (t & K40_MASK);
}

// ============================================================================
// Standard Library Sort
// ============================================================================

/**
 * @brief Sort layer using std::sort with 20-bit key
 * @tparam T Item type
 * @param a Layer to sort
 */
template <typename T>
inline void std_sort20(LayerVec<T>& a) {
    std::sort(a.begin(), a.end(),
              [](const auto& x, const auto& y) { 
                  return getKey20(x) < getKey20(y); 
              });
}

/**
 * @brief Sort layer using std::sort with 40-bit key
 * @tparam T Item type
 * @param a Layer to sort
 */
template <typename T>
inline void std_sort40(LayerVec<T>& a) {
    std::sort(a.begin(), a.end(),
              [](const auto& x, const auto& y) { 
                  return getKey40(x) < getKey40(y); 
              });
}

// ============================================================================
// Radix Sort (kxsort)
// ============================================================================

/**
 * @brief Radix sort traits for 20-bit keys
 * 
 * Defines how to extract individual bytes from 20-bit keys
 * for radix sorting.
 * 
 * @tparam T Item type
 */
template <typename T>
struct Radix20Traits {
    static const int nBytes = 3; ///< Number of bytes in key (20 bits = 3 bytes)
    
    /**
     * @brief Extract k-th byte from item's key
     * @param x Item to extract from
     * @param k Byte index (0 = LSB, 2 = MSB)
     * @return Byte value
     */
    inline uint32_t kth_byte(const T& x, int k) const {
        auto v = getKey20(x);
        return (v >> (8 * k)) & 0xFFu;
    }
    
    /**
     * @brief Compare two items by their keys
     * @param a First item
     * @param b Second item
     * @return true if a < b
     */
    inline bool compare(const T& a, const T& b) const {
        return getKey20(a) < getKey20(b);
    }
};

/**
 * @brief Radix sort traits for 40-bit keys
 * 
 * Defines how to extract individual bytes from 40-bit keys
 * for radix sorting.
 * 
 * @tparam T Item type
 */
template <typename T>
struct Radix40Traits {
    static const int nBytes = 5; ///< Number of bytes in key (40 bits = 5 bytes)
    
    /**
     * @brief Extract k-th byte from item's key
     * @param x Item to extract from
     * @param k Byte index (0 = LSB, 4 = MSB)
     * @return Byte value
     */
    inline uint32_t kth_byte(const T& x, int k) const {
        auto v = getKey40(x);
        return static_cast<uint32_t>((v >> (8 * k)) & 0xFFu);
    }
    
    /**
     * @brief Compare two items by their keys
     * @param a First item
     * @param b Second item
     * @return true if a < b
     */
    inline bool compare(const T& a, const T& b) const {
        return getKey40(a) < getKey40(b);
    }
};

/**
 * @brief Sort layer using radix sort with 20-bit key
 * @tparam T Item type
 * @param a Layer to sort
 */
template <typename T>
inline void kx_sort20(LayerVec<T>& a) {
    kx::radix_sort(a.begin(), a.end(), Radix20Traits<T>{});
}

/**
 * @brief Sort layer using radix sort with 40-bit key
 * @tparam T Item type
 * @param a Layer to sort
 */
template <typename T>
inline void kx_sort40(LayerVec<T>& a) {
    kx::radix_sort(a.begin(), a.end(), Radix40Traits<T>{});
}

// ============================================================================
// Dispatch Functions
// ============================================================================

/**
 * @brief Sort layer with 20-bit key using selected algorithm
 * 
 * Dispatches to std::sort or radix sort based on g_sort_algo.
 * 
 * @tparam T Item type
 * @param a Layer to sort
 */
template <typename T>
inline void sort20(LayerVec<T>& a) {
    if (g_sort_algo == SortAlgo::STD) {
        std_sort20(a);
    } else {
        kx_sort20(a);
    }
}

/**
 * @brief Sort layer with 40-bit key using selected algorithm
 * 
 * Dispatches to std::sort or radix sort based on g_sort_algo.
 * 
 * @tparam T Item type
 * @param a Layer to sort
 */
template <typename T>
inline void sort40(LayerVec<T>& a) {
    if (g_sort_algo == SortAlgo::STD) {
        std_sort40(a);
    } else {
        kx_sort40(a);
    }
}
