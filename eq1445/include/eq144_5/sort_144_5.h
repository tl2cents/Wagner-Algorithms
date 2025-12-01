#pragma once

#include "eq144_5/equihash_144_5.h"
#include "core/sort.h"
#include "core/util.h"

inline constexpr std::size_t EQ1445_COLLISION_BITS = EquihashParams::kCollisionBitLength;
inline constexpr std::size_t EQ1445_FINAL_BITS = EquihashParams::kCollisionBitLength * 2;

template <typename T>
inline auto get_collision_key_144_5(const T &item)
{
    return get_key_bits<T, EQ1445_COLLISION_BITS>(item);
}

template <typename T>
inline auto get_final_key_144_5(const T &item)
{
    return get_key_bits<T, EQ1445_FINAL_BITS>(item);
}

template <typename T>
inline auto getKey24(const T &item)
{
    return get_collision_key_144_5(item);
}

template <typename T>
inline auto getKey48(const T &item)
{
    return get_final_key_144_5(item);
}

// -----------------------------------------------------------------------------
// IV hashing helpers (XIV computation) and keyed bit extraction
// -----------------------------------------------------------------------------

template <size_t Layer>
inline Item0 compute_iv_hash(int seed, const IV<Layer> &iv)
{
    static_assert(Layer <= 5, "IV layer out of range for Equihash(144,5)");
    Item0 acc{};
    std::memset(acc.XOR, 0, sizeof(acc.XOR));
    constexpr size_t kCount = IV<Layer>::NumIndices;
    for (size_t i = 0; i < kCount; ++i)
    {
        const size_t idx = iv.get_index(i);
        const Item0 leaf = compute_ith_item(seed, idx);
        for (size_t b = 0; b < sizeof(acc.XOR); ++b)
        {
            acc.XOR[b] ^= leaf.XOR[b];
        }
    }
    return acc;
}

template <size_t Layer, size_t KeyBits>
inline auto get_iv_key_bits(int seed, const IV<Layer> &iv)
{
    static_assert(KeyBits <= 64, "KeyBits must fit into 64 bits");
    constexpr size_t HASH_BITS = EquihashParams::kLayer0XorBytes * 8;
    constexpr size_t OFFSET_BITS = EquihashParams::kCollisionBitLength * Layer;
    static_assert(OFFSET_BITS + KeyBits <= HASH_BITS,
                  "Requested key window exceeds XIV length");

    using KeyType = std::conditional_t<(KeyBits <= 32), uint32_t, uint64_t>;
    if constexpr (KeyBits == 0)
    {
        return KeyType{0};
    }

    const Item0 xiv = compute_iv_hash<Layer>(seed, iv);
    const uint8_t *bytes = xiv.XOR;

    if constexpr ((OFFSET_BITS % 8 == 0) && (KeyBits % 8 == 0))
    {
        constexpr size_t OFFSET_BYTES = OFFSET_BITS / 8;
        constexpr size_t KEY_BYTES = KeyBits / 8;
        KeyType key = 0;
        std::memcpy(&key, bytes + OFFSET_BYTES, KEY_BYTES);
        return key;
    }
    else
    {
        KeyType key = 0;
        for (size_t bit = 0; bit < KeyBits; ++bit)
        {
            const size_t absolute_bit = OFFSET_BITS + bit;
            const size_t byte_index = absolute_bit / 8;
            const size_t bit_index = absolute_bit % 8;
            const KeyType bit_val = static_cast<KeyType>((bytes[byte_index] >> bit_index) & 0x1u);
            key |= (bit_val << bit);
        }
        return key;
    }
}

template <size_t Layer, typename HasherT>
inline Item0 compute_iv_hash_fast(HasherT& H, const IV<Layer> &iv)
{
    static_assert(Layer <= 5, "IV layer out of range for Equihash(144,5)");
    Item0 acc{};
    std::memset(acc.XOR, 0, sizeof(acc.XOR));
    constexpr size_t kCount = IV<Layer>::NumIndices;
    for (size_t i = 0; i < kCount; ++i)
    {
        const size_t idx = iv.get_index(i);
        const Item0 leaf = _compute_ith_item_fast<EquihashParams, Item0>(H, idx);
        for (size_t b = 0; b < sizeof(acc.XOR); ++b)
        {
            acc.XOR[b] ^= leaf.XOR[b];
        }
    }
    return acc;
}

template <size_t Layer, size_t KeyBits, typename HasherT>
inline auto get_iv_key_bits_fast(HasherT& H, const IV<Layer> &iv)
{
    static_assert(KeyBits <= 64, "KeyBits must fit into 64 bits");
    constexpr size_t HASH_BITS = EquihashParams::kLayer0XorBytes * 8;
    constexpr size_t OFFSET_BITS = EquihashParams::kCollisionBitLength * Layer;
    static_assert(OFFSET_BITS + KeyBits <= HASH_BITS,
                  "Requested key window exceeds XIV length");

    using KeyType = std::conditional_t<(KeyBits <= 32), uint32_t, uint64_t>;
    if constexpr (KeyBits == 0)
    {
        return KeyType{0};
    }

    const Item0 xiv = compute_iv_hash_fast<Layer>(H, iv);
    const uint8_t *bytes = xiv.XOR;

    if constexpr ((OFFSET_BITS % 8 == 0) && (KeyBits % 8 == 0))
    {
        constexpr size_t OFFSET_BYTES = OFFSET_BITS / 8;
        constexpr size_t KEY_BYTES = KeyBits / 8;
        KeyType key = 0;
        std::memcpy(&key, bytes + OFFSET_BYTES, KEY_BYTES);
        return key;
    }
    else
    {
        KeyType key = 0;
        for (size_t bit = 0; bit < KeyBits; ++bit)
        {
            const size_t absolute_bit = OFFSET_BITS + bit;
            const size_t byte_index = absolute_bit / 8;
            const size_t bit_index = absolute_bit % 8;
            const KeyType bit_val = static_cast<KeyType>((bytes[byte_index] >> bit_index) & 0x1u);
            key |= (bit_val << bit);
        }
        return key;
    }
}

template <size_t Layer>
inline auto getKey24(int seed, const IV<Layer> &iv)
{
    return get_iv_key_bits<Layer, EQ1445_COLLISION_BITS>(seed, iv);
}

template <size_t Layer>
inline auto getKey48(int seed, const IV<Layer> &iv)
{
    return get_iv_key_bits<Layer, EQ1445_FINAL_BITS>(seed, iv);
}

// Sorting helpers for IV layers (seed-dependent keys) -----------------------

template <size_t Layer, size_t KeyBits, typename HasherT>
struct IVRadixKeyTraitsFast
{
    HasherT* H;
    static constexpr int nBytes = static_cast<int>((KeyBits + 7) / 8);

    inline uint32_t kth_byte(const IV<Layer> &x, int k) const
    {
        using KeyType = decltype(get_iv_key_bits_fast<Layer, KeyBits>(*H, x));
        const KeyType key = get_iv_key_bits_fast<Layer, KeyBits>(*H, x);
        return static_cast<uint32_t>((key >> (8 * k)) & 0xFFu);
    }

    inline bool compare(const IV<Layer> &a, const IV<Layer> &b) const
    {
        return get_iv_key_bits_fast<Layer, KeyBits>(*H, a) <
               get_iv_key_bits_fast<Layer, KeyBits>(*H, b);
    }
};

template <size_t Layer, size_t KeyBits, typename HasherT>
inline void sort_iv_by_key_fast(LayerIV<Layer> &layer, HasherT& H)
{
    if (g_sort_algo == SortAlgo::STD)
    {
        std::sort(layer.begin(), layer.end(),
                  [&H](const IV<Layer> &lhs, const IV<Layer> &rhs)
                  {
                      return get_iv_key_bits_fast<Layer, KeyBits>(H, lhs) <
                             get_iv_key_bits_fast<Layer, KeyBits>(H, rhs);
                  });
    }
    else
    {
        kx::radix_sort(layer.begin(), layer.end(),
                       IVRadixKeyTraitsFast<Layer, KeyBits, HasherT>{&H});
    }
}

template <size_t Layer, size_t KeyBits>
struct IVRadixKeyTraits
{
    int seed;
    static constexpr int nBytes = static_cast<int>((KeyBits + 7) / 8);

    inline uint32_t kth_byte(const IV<Layer> &x, int k) const
    {
        using KeyType = decltype(get_iv_key_bits<Layer, KeyBits>(seed, x));
        const KeyType key = get_iv_key_bits<Layer, KeyBits>(seed, x);
        return static_cast<uint32_t>((key >> (8 * k)) & 0xFFu);
    }

    inline bool compare(const IV<Layer> &a, const IV<Layer> &b) const
    {
        return get_iv_key_bits<Layer, KeyBits>(seed, a) <
               get_iv_key_bits<Layer, KeyBits>(seed, b);
    }
};

template <size_t Layer, size_t KeyBits>
inline void sort_iv_by_key(LayerIV<Layer> &layer, int seed)
{
    if (g_sort_algo == SortAlgo::STD)
    {
        std::sort(layer.begin(), layer.end(),
                  [seed](const IV<Layer> &lhs, const IV<Layer> &rhs)
                  {
                      return get_iv_key_bits<Layer, KeyBits>(seed, lhs) <
                             get_iv_key_bits<Layer, KeyBits>(seed, rhs);
                  });
    }
    else
    {
        kx::radix_sort(layer.begin(), layer.end(),
                       IVRadixKeyTraits<Layer, KeyBits>{seed});
    }
}

template <size_t Layer>
inline void sort24_IV(LayerIV<Layer> &layer, int seed)
{
    sort_iv_by_key<Layer, EQ1445_COLLISION_BITS>(layer, seed);
}

template <size_t Layer>
inline void sort48_IV(LayerIV<Layer> &layer, int seed)
{
    sort_iv_by_key<Layer, EQ1445_FINAL_BITS>(layer, seed);
}

// =============================================================================
// IV_Key: Set key for IndexVectorKey from its indices
// =============================================================================

template <size_t Layer, size_t KeyBits, typename KeyType, typename HasherT>
inline void SetIV_Key_Fast(HasherT& H, IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType> &ivkey)
{
    static_assert(KeyBits <= 64, "KeyBits must fit into 64 bits");
    
    // Compute XIV hash from all indices
    Item0 acc{};
    std::memset(acc.XOR, 0, sizeof(acc.XOR));
    constexpr size_t kCount = (1ULL << Layer);
    
    for (size_t i = 0; i < kCount; ++i)
    {
        const size_t idx = ivkey.get_index(i);
        const Item0 leaf = _compute_ith_item_fast<EquihashParams, Item0>(H, idx);
        for (size_t b = 0; b < sizeof(acc.XOR); ++b)
        {
            acc.XOR[b] ^= leaf.XOR[b];
        }
    }
    
    // Extract key bits from XIV
    constexpr size_t HASH_BITS = EquihashParams::kLayer0XorBytes * 8;
    constexpr size_t OFFSET_BITS = EquihashParams::kCollisionBitLength * Layer;
    static_assert(OFFSET_BITS + KeyBits <= HASH_BITS,
                  "Requested key window exceeds XIV length");
    
    if constexpr (KeyBits == 0)
    {
        ivkey.key = KeyType{0};
        return;
    }
    
    const uint8_t *bytes = acc.XOR;
    
    if constexpr ((OFFSET_BITS % 8 == 0) && (KeyBits % 8 == 0))
    {
        constexpr size_t OFFSET_BYTES = OFFSET_BITS / 8;
        constexpr size_t KEY_BYTES = KeyBits / 8;
        KeyType key = 0;
        std::memcpy(&key, bytes + OFFSET_BYTES, KEY_BYTES);
        ivkey.key = key;
    }
    else
    {
        KeyType key = 0;
        for (size_t bit = 0; bit < KeyBits; ++bit)
        {
            const size_t absolute_bit = OFFSET_BITS + bit;
            const size_t byte_index = absolute_bit / 8;
            const size_t bit_index = absolute_bit % 8;
            const KeyType bit_val = static_cast<KeyType>((bytes[byte_index] >> bit_index) & 0x1u);
            key |= (bit_val << bit);
        }
        ivkey.key = key;
    }
}

// SetIV_Key: Compute and set the collision key for an IV_Key from its indices
// This function should be called after merge_ivkey to populate the key field
template <size_t Layer, size_t KeyBits, typename KeyType>
inline void SetIV_Key(int seed, IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType> &ivkey)
{
    // Initialize Hasher
    uint8_t headernonce[140];
    std::memset(headernonce, 0, sizeof(headernonce));
    uint32_t *nonce_ptr = reinterpret_cast<uint32_t *>(headernonce + 108);
    *nonce_ptr = seed;
    uint8_t dummy_nonce[32];
    std::memset(dummy_nonce, 0, sizeof(dummy_nonce));

    ZcashEquihashHasher H;
    H.init_midstate(headernonce, sizeof(headernonce), dummy_nonce,
                    EquihashParams::N,
                    EquihashParams::K);

    SetIV_Key_Fast<Layer, KeyBits, KeyType>(H, ivkey);
}

// Simple cached hasher to avoid re-computing Blake2b for adjacent indices sharing the same hash block
struct CachedHasher
{
    ZcashEquihashHasher &H;
    uint32_t last_pair_idx = 0xFFFFFFFF;
    uint8_t last_out[ZcashEquihashHasher::OUT_LEN];

    CachedHasher(ZcashEquihashHasher &h) : H(h) {}

    void hash_index(uint32_t pair_idx, uint8_t *out)
    {
        if (pair_idx == last_pair_idx)
        {
            std::memcpy(out, last_out, ZcashEquihashHasher::OUT_LEN);
        }
        else
        {
            H.hash_index(pair_idx, out);
            last_pair_idx = pair_idx;
            std::memcpy(last_out, out, ZcashEquihashHasher::OUT_LEN);
        }
    }
};

// Batch SetIV_Key for a layer
template <size_t Layer, size_t KeyBits, typename KeyType>
inline void SetIV_Key_Batch(int seed, LayerVec<IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType>> &layer)
{
    // Initialize Hasher once
    uint8_t headernonce[140];
    std::memset(headernonce, 0, sizeof(headernonce));
    uint32_t *nonce_ptr = reinterpret_cast<uint32_t *>(headernonce + 108);
    *nonce_ptr = seed;
    uint8_t dummy_nonce[32];
    std::memset(dummy_nonce, 0, sizeof(dummy_nonce));

    ZcashEquihashHasher H;
    H.init_midstate(headernonce, sizeof(headernonce), dummy_nonce,
                    EquihashParams::N,
                    EquihashParams::K);

    CachedHasher cachedH(H);

    for (auto &ivkey : layer)
    {
        SetIV_Key_Fast<Layer, KeyBits, KeyType>(cachedH, ivkey);
    }
}


// Key extraction for IV_Key (no computation needed - just return the stored key)
template <size_t Layer, typename KeyType>
inline KeyType getKey_IVKey(int /*seed*/, const IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType> &ivkey)
{
    return ivkey.key;
}

// Sorting helpers for IV_Key layers (uses precomputed keys)
template <size_t Layer, typename KeyType>
struct IVKeyRadixKeyTraits
{
    static constexpr int nBytes = static_cast<int>(sizeof(KeyType));
    
    inline uint32_t kth_byte(const IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType> &x, int k) const
    {
        return static_cast<uint32_t>((x.key >> (8 * k)) & 0xFFu);
    }
    
    inline bool compare(const IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType> &a,
                       const IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType> &b) const
    {
        return a.key < b.key;
    }
};

template <size_t Layer, typename KeyType>
inline void sort_ivkey_by_key(LayerVec<IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType>> &layer, int /*seed*/)
{
    if (g_sort_algo == SortAlgo::STD)
    {
        std::sort(layer.begin(), layer.end(),
                  [](const IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType> &lhs,
                     const IndexVectorKey<EquihashParams::kIndexBytes, Layer, KeyType> &rhs)
                  {
                      return lhs.key < rhs.key;
                  });
    }
    else
    {
        kx::radix_sort(layer.begin(), layer.end(),
                       IVKeyRadixKeyTraits<Layer, KeyType>{});
    }
}

// 24-bit key sorting for IV_Key
template <size_t Layer>
inline void sort24_IVKey(LayerVec<IndexVectorKey<EquihashParams::kIndexBytes, Layer, uint32_t>> &layer, int seed)
{
    sort_ivkey_by_key<Layer, uint32_t>(layer, seed);
}

// 48-bit key sorting for IV_Key
template <size_t Layer>
inline void sort48_IVKey(LayerVec<IndexVectorKey<EquihashParams::kIndexBytes, Layer, uint64_t>> &layer, int seed)
{
    sort_ivkey_by_key<Layer, uint64_t>(layer, seed);
}

// Sorting wrappers ------------------------------------------------------------

template <typename T>
inline void sort24(LayerVec<T> &layer)
{
    sort_layer_by_key<T, EQ1445_COLLISION_BITS>(layer);
}

template <typename T>
inline void sort48(LayerVec<T> &layer)
{
    sort_layer_by_key<T, EQ1445_FINAL_BITS>(layer);
}
