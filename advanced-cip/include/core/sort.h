#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "core/equihash_base.h"
#include "kxsort.h"

enum class SortAlgo
{
    STD,
    KXSORT
};

extern SortAlgo g_sort_algo;
extern bool g_verbose;

namespace equihash
{
namespace detail
{
template <typename KeyT>
constexpr KeyT bit_mask(std::size_t bits)
{
    if (bits == 0)
        return KeyT{0};
    constexpr std::size_t total = sizeof(KeyT) * 8;
    if (bits >= total)
        return std::numeric_limits<KeyT>::max();
    return static_cast<KeyT>((KeyT{1} << bits) - 1);
}
} // namespace detail
} // namespace equihash

template <typename Item, std::size_t KeyBits>
inline auto get_key_bits(const Item &item)
{
    using KeyType = std::conditional_t<(KeyBits <= 32), uint32_t, uint64_t>;
    KeyType value = 0;
    
    if constexpr (sizeof(item.XOR) >= sizeof(KeyType))
    {
        std::memcpy(&value, item.XOR, sizeof(KeyType));
    }
    else
    {
        constexpr std::size_t bytes_needed = (KeyBits + 7) / 8;
        constexpr std::size_t bytes_to_copy = bytes_needed < sizeof(item.XOR)
                                                  ? bytes_needed
                                                  : sizeof(item.XOR);
        std::memcpy(&value, item.XOR, bytes_to_copy);
    }

    if constexpr (KeyBits == 0)
    {
        return KeyType{0};
    }
    else if constexpr (KeyBits >= sizeof(KeyType) * 8)
    {
        return value;
    }
    else
    {
        return static_cast<KeyType>(value & equihash::detail::bit_mask<KeyType>(KeyBits));
    }
}

template <typename Item, std::size_t KeyBits>
struct RadixKeyTraits
{
    static constexpr int nBytes = static_cast<int>((KeyBits + 7) / 8);

    inline uint32_t kth_byte(const Item &x, int k) const
    {
        auto v = get_key_bits<Item, KeyBits>(x);
        return static_cast<uint32_t>((v >> (8 * k)) & 0xFFu);
    }

    inline bool compare(const Item &a, const Item &b) const
    {
        return get_key_bits<Item, KeyBits>(a) < get_key_bits<Item, KeyBits>(b);
    }
};

template <typename Item, std::size_t KeyBits>
inline void std_sort_by_key(LayerVec<Item> &layer)
{
    std::sort(layer.begin(), layer.end(),
              [](const Item &lhs, const Item &rhs)
              {
                  return get_key_bits<Item, KeyBits>(lhs) <
                         get_key_bits<Item, KeyBits>(rhs);
              });
}

template <typename Item, std::size_t KeyBits>
inline void kx_sort_by_key(LayerVec<Item> &layer)
{
    kx::radix_sort(layer.begin(), layer.end(), RadixKeyTraits<Item, KeyBits>{});
}

template <typename Item, std::size_t KeyBits>
inline void sort_layer_by_key(LayerVec<Item> &layer)
{
    if (g_sort_algo == SortAlgo::STD)
    {
        std_sort_by_key<Item, KeyBits>(layer);
    }
    else
    {
        kx_sort_by_key<Item, KeyBits>(layer);
    }
}
