#pragma once

#include "core/equihash_base.h"

namespace equihash
{
struct EquihashParams_200_9
{
    static constexpr std::uint32_t N = 200;
    static constexpr std::uint32_t K = 9;

    static constexpr std::size_t kLayer0XorBytes = 25;
    static constexpr std::size_t kLayer1XorBytes = 23;
    static constexpr std::size_t kLayer2XorBytes = 20;
    static constexpr std::size_t kLayer3XorBytes = 18;
    static constexpr std::size_t kLayer4XorBytes = 15;
    static constexpr std::size_t kLayer5XorBytes = 13;
    static constexpr std::size_t kLayer6XorBytes = 10;
    static constexpr std::size_t kLayer7XorBytes = 8;
    static constexpr std::size_t kLayer8XorBytes = 5;
    static constexpr std::size_t kLayer9XorBytes = 3;

    static constexpr std::size_t kIndexBytes = 3;
    static constexpr std::size_t kCollisionBitLength = 20;

    static constexpr std::uint32_t kLeafCountHalf = 1u << 20;
    static constexpr std::uint32_t kLeafCountFull = kLeafCountHalf * 2;

    static constexpr std::size_t kMaxListSize = 2200000;
    static constexpr std::size_t kInitialListSize = 2097152;
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
using IPDiskManifest = equihash::IPDiskManifestT<Item_IP, 8>;

inline constexpr std::size_t MAX_LIST_SIZE = EquihashParams::kMaxListSize;
inline constexpr std::size_t INITIAL_LIST_SIZE = EquihashParams::kInitialListSize;
