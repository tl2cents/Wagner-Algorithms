#pragma once

#include "eq144_5/equihash_144_5.h"
#include "core/sort.h"

inline constexpr std::size_t EQ1445_COLLISION_BITS = EquihashParams::kCollisionBitLength;
inline constexpr std::size_t EQ1445_FINAL_BITS = EquihashParams::kCollisionBitLength * 2;

// Key helpers ----------------------------------------------------------------

template <typename T>
inline auto get_collision_key(const T &item)
{
    return get_key_bits<T, EQ1445_COLLISION_BITS>(item);
}

template <typename T>
inline auto get_final_key(const T &item)
{
    return get_key_bits<T, EQ1445_FINAL_BITS>(item);
}

// Backwards-compatible names --------------------------------------------------

template <typename T>
inline auto getKey24(const T &item)
{
    return get_collision_key(item);
}

template <typename T>
inline auto getKey48(const T &item)
{
    return get_final_key(item);
}

// Sorting wrappers ------------------------------------------------------------

template <typename T>
inline void sort_collision(LayerVec<T> &layer)
{
    sort_layer_by_key<T, EQ1445_COLLISION_BITS>(layer);
}

template <typename T>
inline void sort_final(LayerVec<T> &layer)
{
    sort_layer_by_key<T, EQ1445_FINAL_BITS>(layer);
}

template <typename T>
inline void sort24(LayerVec<T> &layer)
{
    sort_collision(layer);
}

template <typename T>
inline void sort48(LayerVec<T> &layer)
{
    sort_final(layer);
}
