#pragma once

#include "core/equihash_base.h"

namespace equihash
{
struct EquihashParams_144_5
{
    static constexpr std::uint32_t N = 144;
    static constexpr std::uint32_t K = 5;

    static constexpr std::size_t kLayer0XorBytes = 18; // ceil(144 / 8)
    static constexpr std::size_t kLayer1XorBytes = 15; // ceil(120 / 8)
    static constexpr std::size_t kLayer2XorBytes = 12; // ceil(96 / 8)
    static constexpr std::size_t kLayer3XorBytes = 9;  // ceil(72 / 8)
    static constexpr std::size_t kLayer4XorBytes = 6;  // ceil(48 / 8)
    static constexpr std::size_t kLayer5XorBytes = 3;  // ceil(24 / 8)

    static constexpr std::size_t kIndexBytes = 4; // Larger leaf space requires 32-bit indices
    static constexpr std::size_t kCollisionBitLength = 24; // N / (K + 1)

    static constexpr std::uint32_t kLeafCountHalf = 1u << kCollisionBitLength; // 2^24
    static constexpr std::uint32_t kLeafCountFull = kLeafCountHalf * 2u;         // 2^25

    // 35000000 slightly greater than 2^25 = 33554432
    static constexpr std::size_t kMaxListSize = 34000000;
    static constexpr std::size_t kInitialListSize = kLeafCountFull;
};
} // namespace equihash

using EquihashParams = equihash::EquihashParams_144_5;

using Item0 = ItemVal<EquihashParams::kLayer0XorBytes>;
using Item1 = ItemVal<EquihashParams::kLayer1XorBytes>;
using Item2 = ItemVal<EquihashParams::kLayer2XorBytes>;
using Item3 = ItemVal<EquihashParams::kLayer3XorBytes>;
using Item4 = ItemVal<EquihashParams::kLayer4XorBytes>;
using Item5 = ItemVal<EquihashParams::kLayer5XorBytes>;

using Item0_IDX = ItemValIdx<EquihashParams::kLayer0XorBytes, EquihashParams::kIndexBytes>;
using Item1_IDX = ItemValIdx<EquihashParams::kLayer1XorBytes, EquihashParams::kIndexBytes>;
using Item2_IDX = ItemValIdx<EquihashParams::kLayer2XorBytes, EquihashParams::kIndexBytes>;
using Item3_IDX = ItemValIdx<EquihashParams::kLayer3XorBytes, EquihashParams::kIndexBytes>;
using Item4_IDX = ItemValIdx<EquihashParams::kLayer4XorBytes, EquihashParams::kIndexBytes>;
using Item5_IDX = ItemValIdx<EquihashParams::kLayer5XorBytes, EquihashParams::kIndexBytes>;

using Item_IP = ItemIP<EquihashParams::kIndexBytes>;

// IV types: Index Vector only
using IV0 = IndexVector<EquihashParams::kIndexBytes, 0>;
using IV1 = IndexVector<EquihashParams::kIndexBytes, 1>;
using IV2 = IndexVector<EquihashParams::kIndexBytes, 2>;
using IV3 = IndexVector<EquihashParams::kIndexBytes, 3>;
using IV4 = IndexVector<EquihashParams::kIndexBytes, 4>;
using IV5 = IndexVector<EquihashParams::kIndexBytes, 5>;

using Layer0 = LayerVec<Item0>;
using Layer1 = LayerVec<Item1>;
using Layer2 = LayerVec<Item2>;
using Layer3 = LayerVec<Item3>;
using Layer4 = LayerVec<Item4>;
using Layer5 = LayerVec<Item5>;

using Layer0_IDX = LayerVec<Item0_IDX>;
using Layer1_IDX = LayerVec<Item1_IDX>;
using Layer2_IDX = LayerVec<Item2_IDX>;
using Layer3_IDX = LayerVec<Item3_IDX>;
using Layer4_IDX = LayerVec<Item4_IDX>;
using Layer5_IDX = LayerVec<Item5_IDX>;
using Layer_IP = LayerVec<Item_IP>;

using Layer0_IV = LayerVec<IV0>;
using Layer1_IV = LayerVec<IV1>;
using Layer2_IV = LayerVec<IV2>;
using Layer3_IV = LayerVec<IV3>;
using Layer4_IV = LayerVec<IV4>;
using Layer5_IV = LayerVec<IV5>;

using IPDiskMeta = equihash::IPDiskMetaT<Item_IP>;
using IPDiskManifest = equihash::IPDiskManifestT<Item_IP, 4>; // Stores IP1-IP4 on disk

inline constexpr std::size_t MAX_LIST_SIZE = EquihashParams::kMaxListSize;
inline constexpr std::size_t INITIAL_LIST_SIZE = EquihashParams::kInitialListSize;

// Template aliases for indexed access
template<size_t I> struct ItemType;
template<> struct ItemType<0> { using type = Item0; };
template<> struct ItemType<1> { using type = Item1; };
template<> struct ItemType<2> { using type = Item2; };
template<> struct ItemType<3> { using type = Item3; };
template<> struct ItemType<4> { using type = Item4; };
template<> struct ItemType<5> { using type = Item5; };
template<size_t I> using Item = typename ItemType<I>::type;

template<size_t I> struct ItemIDXType;
template<> struct ItemIDXType<0> { using type = Item0_IDX; };
template<> struct ItemIDXType<1> { using type = Item1_IDX; };
template<> struct ItemIDXType<2> { using type = Item2_IDX; };
template<> struct ItemIDXType<3> { using type = Item3_IDX; };
template<> struct ItemIDXType<4> { using type = Item4_IDX; };
template<> struct ItemIDXType<5> { using type = Item5_IDX; };
template<size_t I> using ItemIDX = typename ItemIDXType<I>::type;

template<size_t I> struct LayerType;
template<> struct LayerType<0> { using type = Layer0; };
template<> struct LayerType<1> { using type = Layer1; };
template<> struct LayerType<2> { using type = Layer2; };
template<> struct LayerType<3> { using type = Layer3; };
template<> struct LayerType<4> { using type = Layer4; };
template<> struct LayerType<5> { using type = Layer5; };
template<size_t I> using Layer = typename LayerType<I>::type;

template<size_t I> struct LayerIDXType;
template<> struct LayerIDXType<0> { using type = Layer0_IDX; };
template<> struct LayerIDXType<1> { using type = Layer1_IDX; };
template<> struct LayerIDXType<2> { using type = Layer2_IDX; };
template<> struct LayerIDXType<3> { using type = Layer3_IDX; };
template<> struct LayerIDXType<4> { using type = Layer4_IDX; };
template<> struct LayerIDXType<5> { using type = Layer5_IDX; };
template<size_t I> using LayerIDX = typename LayerIDXType<I>::type;

template<size_t I> struct IVType;
template<> struct IVType<0> { using type = IV0; };
template<> struct IVType<1> { using type = IV1; };
template<> struct IVType<2> { using type = IV2; };
template<> struct IVType<3> { using type = IV3; };
template<> struct IVType<4> { using type = IV4; };
template<> struct IVType<5> { using type = IV5; };
template<size_t I> using IV = typename IVType<I>::type;

template<size_t I> struct Layer_IVType;
template<> struct Layer_IVType<0> { using type = Layer0_IV; };
template<> struct Layer_IVType<1> { using type = Layer1_IV; };
template<> struct Layer_IVType<2> { using type = Layer2_IV; };
template<> struct Layer_IVType<3> { using type = Layer3_IV; };
template<> struct Layer_IVType<4> { using type = Layer4_IV; };
template<> struct Layer_IVType<5> { using type = Layer5_IV; };
template<size_t I> using LayerIV = typename Layer_IVType<I>::type;
