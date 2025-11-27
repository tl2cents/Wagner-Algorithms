#pragma once

#include "eq144_5/equihash_144_5.h"
#include "core/sort.h"

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
