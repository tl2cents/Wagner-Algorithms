#pragma once

#include "core/equihash_base.h"

namespace equihash
{
struct EquihashParams_200_9
{
    static constexpr std::uint32_t N = 200;
    static constexpr std::uint32_t K = 9;

    static constexpr std::size_t kLayer0XorBytes = 25; // ceil(200 / 8)
    static constexpr std::size_t kLayer1XorBytes = 23; // ceil(180 / 8)
    static constexpr std::size_t kLayer2XorBytes = 20; // ceil(160 / 8)
    static constexpr std::size_t kLayer3XorBytes = 18; // ceil(144 / 8)
    static constexpr std::size_t kLayer4XorBytes = 15; // ceil(120 / 8)
    static constexpr std::size_t kLayer5XorBytes = 13; // ceil(104 / 8)
    static constexpr std::size_t kLayer6XorBytes = 10; // ceil(80 / 8)
    static constexpr std::size_t kLayer7XorBytes = 8;  // ceil(64 / 8)
    static constexpr std::size_t kLayer8XorBytes = 5;  // ceil(40 / 8)
    static constexpr std::size_t kLayer9XorBytes = 3;  // ceil(24 / 8)

    static constexpr std::size_t kIndexBytes = 3;            // 24-bit indices cover the leaf space
    static constexpr std::size_t kCollisionBitLength = 20;   // N / (K + 1)

    static constexpr std::uint32_t kLeafCountHalf = 1u << kCollisionBitLength; // 2^20
    static constexpr std::uint32_t kLeafCountFull = kLeafCountHalf * 2u;      // 2^21

    // 2200000 slightly greater than 2^21 = 2097152
    static constexpr std::size_t kMaxListSize = 2200000;
    static constexpr std::size_t kInitialListSize = kLeafCountFull;
};
} // namespace equihash

using EquihashParams = equihash::EquihashParams_200_9;

using Item0 = ItemVal<EquihashParams::kLayer0XorBytes>;
using Item1 = ItemVal<EquihashParams::kLayer1XorBytes>;
using Item2 = ItemVal<EquihashParams::kLayer2XorBytes>;
using Item3 = ItemVal<EquihashParams::kLayer3XorBytes>;
using Item4 = ItemVal<EquihashParams::kLayer4XorBytes>;
using Item5 = ItemVal<EquihashParams::kLayer5XorBytes>;
using Item6 = ItemVal<EquihashParams::kLayer6XorBytes>;
using Item7 = ItemVal<EquihashParams::kLayer7XorBytes>;
using Item8 = ItemVal<EquihashParams::kLayer8XorBytes>;

using Item0_IDX = ItemValIdx<EquihashParams::kLayer0XorBytes, EquihashParams::kIndexBytes>;
using Item1_IDX = ItemValIdx<EquihashParams::kLayer1XorBytes, EquihashParams::kIndexBytes>;
using Item2_IDX = ItemValIdx<EquihashParams::kLayer2XorBytes, EquihashParams::kIndexBytes>;
using Item3_IDX = ItemValIdx<EquihashParams::kLayer3XorBytes, EquihashParams::kIndexBytes>;
using Item4_IDX = ItemValIdx<EquihashParams::kLayer4XorBytes, EquihashParams::kIndexBytes>;
using Item5_IDX = ItemValIdx<EquihashParams::kLayer5XorBytes, EquihashParams::kIndexBytes>;
using Item6_IDX = ItemValIdx<EquihashParams::kLayer6XorBytes, EquihashParams::kIndexBytes>;
using Item7_IDX = ItemValIdx<EquihashParams::kLayer7XorBytes, EquihashParams::kIndexBytes>;
using Item8_IDX = ItemValIdx<EquihashParams::kLayer8XorBytes, EquihashParams::kIndexBytes>;
using Item9_IDX = ItemValIdx<EquihashParams::kLayer9XorBytes, EquihashParams::kIndexBytes>;

using Item_IP = ItemIP<EquihashParams::kIndexBytes>;

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
using Layer_IP = LayerVec<Item_IP>;

using IPDiskMeta = equihash::IPDiskMetaT<Item_IP>;
using IPDiskManifest = equihash::IPDiskManifestT<Item_IP, 8>; // Stores IP1-IP8 on disk

inline constexpr std::size_t MAX_LIST_SIZE = EquihashParams::kMaxListSize;
inline constexpr std::size_t INITIAL_LIST_SIZE = EquihashParams::kInitialListSize;

// Template aliases for indexed access
template <size_t I> struct ItemType;
template <> struct ItemType<0> { using type = Item0; };
template <> struct ItemType<1> { using type = Item1; };
template <> struct ItemType<2> { using type = Item2; };
template <> struct ItemType<3> { using type = Item3; };
template <> struct ItemType<4> { using type = Item4; };
template <> struct ItemType<5> { using type = Item5; };
template <> struct ItemType<6> { using type = Item6; };
template <> struct ItemType<7> { using type = Item7; };
template <> struct ItemType<8> { using type = Item8; };
template <size_t I> using Item = typename ItemType<I>::type;

template <size_t I> struct ItemIDXType;
template <> struct ItemIDXType<0> { using type = Item0_IDX; };
template <> struct ItemIDXType<1> { using type = Item1_IDX; };
template <> struct ItemIDXType<2> { using type = Item2_IDX; };
template <> struct ItemIDXType<3> { using type = Item3_IDX; };
template <> struct ItemIDXType<4> { using type = Item4_IDX; };
template <> struct ItemIDXType<5> { using type = Item5_IDX; };
template <> struct ItemIDXType<6> { using type = Item6_IDX; };
template <> struct ItemIDXType<7> { using type = Item7_IDX; };
template <> struct ItemIDXType<8> { using type = Item8_IDX; };
template <> struct ItemIDXType<9> { using type = Item9_IDX; };
template <size_t I> using ItemIDX = typename ItemIDXType<I>::type;

template <size_t I> struct LayerType;
template <> struct LayerType<0> { using type = Layer0; };
template <> struct LayerType<1> { using type = Layer1; };
template <> struct LayerType<2> { using type = Layer2; };
template <> struct LayerType<3> { using type = Layer3; };
template <> struct LayerType<4> { using type = Layer4; };
template <> struct LayerType<5> { using type = Layer5; };
template <> struct LayerType<6> { using type = Layer6; };
template <> struct LayerType<7> { using type = Layer7; };
template <> struct LayerType<8> { using type = Layer8; };
template <size_t I> using Layer = typename LayerType<I>::type;

template <size_t I> struct LayerIDXType;
template <> struct LayerIDXType<0> { using type = Layer0_IDX; };
template <> struct LayerIDXType<1> { using type = Layer1_IDX; };
template <> struct LayerIDXType<2> { using type = Layer2_IDX; };
template <> struct LayerIDXType<3> { using type = Layer3_IDX; };
template <> struct LayerIDXType<4> { using type = Layer4_IDX; };
template <> struct LayerIDXType<5> { using type = Layer5_IDX; };
template <> struct LayerIDXType<6> { using type = Layer6_IDX; };
template <> struct LayerIDXType<7> { using type = Layer7_IDX; };
template <> struct LayerIDXType<8> { using type = Layer8_IDX; };
template <> struct LayerIDXType<9> { using type = Layer9_IDX; };
template <size_t I> using LayerIDX = typename LayerIDXType<I>::type;
