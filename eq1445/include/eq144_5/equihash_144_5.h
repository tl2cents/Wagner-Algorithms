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

using IPDiskMeta = equihash::IPDiskMetaT<Item_IP>;
using IPDiskManifest = equihash::IPDiskManifestT<Item_IP, 4>; // Stores IP1-IP4 on disk

inline constexpr std::size_t MAX_LIST_SIZE = EquihashParams::kMaxListSize;
inline constexpr std::size_t INITIAL_LIST_SIZE = EquihashParams::kInitialListSize;
