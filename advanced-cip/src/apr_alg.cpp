#include "advanced_pr.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

// verbose-guarded logging helper
#define IFV if (g_verbose)
// ------------------- Tunables -------------------
#define MAX_LIST_SIZE 2200000
#define INITIAL_LIST_SIZE 2097152

// Compatibility constants used elsewhere in the codebase
inline uint64_t MAX_IP_MEM_BYTES = MAX_LIST_SIZE * sizeof(Item_IP);
inline uint64_t MAX_ITEM_MEM_BYTES = MAX_LIST_SIZE * sizeof(Item0_IDX);
// We can use a little less memory, i.e., `MAX_LIST_SIZE * sizeof(Item0_IDX)` but it will complicate memory management and other unconveniences.
// The peak memory of performing merge_7_ip_inplace is Layer7 + IP6 + IP7 + IP8 in our impl, i.e., the following peak memory to keep things simple (extra memory of 2MB)
[[maybe_unused]] inline uint64_t MAX_MEM_BYTES_APR5 = MAX_LIST_SIZE * sizeof(Item7_IDX) + 3 * MAX_LIST_SIZE * sizeof(Item_IP);
// The same aplies for advanced_cip_pr_4
// inline uint64_t MAX_MEM_BYTES_APR4 = MAX_LIST_SIZE * sizeof(Item7_IDX) + 4 * MAX_LIST_SIZE * sizeof(Item_IP);

// Note: Our implementation wastes some memory for simplicity. This is why our advanced cip-pr algorithm has a higher peak memory usage than the cip-pr algorithm. For example, we stores index as 3 bytes (> 21 bits). The actual memory usage can be slightly reduced by more complex memory management.

SortAlgo g_sort_algo = SortAlgo::KXSORT;
bool g_verbose = true;

static inline size_t idx_size_for_layer(int h)
{
    switch (h)
    {
    case 0:
        return sizeof(Item0_IDX);
    case 1:
        return sizeof(Item1_IDX);
    case 2:
        return sizeof(Item2_IDX);
    case 3:
        return sizeof(Item3_IDX);
    case 4:
        return sizeof(Item4_IDX);
    case 5:
        return sizeof(Item5_IDX);
    case 6:
        return sizeof(Item6_IDX);
    case 7:
        return sizeof(Item7_IDX);
    case 8:
    default:
        return sizeof(Item8_IDX);
    }
}

size_t calc_apr_mem_bytes(int switch_h)
{
    int sh = switch_h;
    if (sh < 0)
        sh = 0;
    if (sh > 8)
        sh = 8;

    // Advanced CIP at switching height h, peak mem ~ L_{k-2} + IP_{k-1}..IP_{h+1}
    // For k=9, L_{k-2} = L7_IDX and there are (8 - h) IP arrays stored.
    // Special cases:
    //   h=0 => plain CIP (items + 8 IP arrays)
    //   h=8 => pure PR  (only item buffer for recover_IP)
    size_t peak_bytes = 0;
    if (sh == 0)
    {
        peak_bytes = MAX_ITEM_MEM_BYTES + 8 * MAX_IP_MEM_BYTES;
    }
    else if (sh == 8)
    {
        peak_bytes = MAX_ITEM_MEM_BYTES;
    }
    else
    {
        peak_bytes = MAX_LIST_SIZE * sizeof(Item7_IDX) + static_cast<size_t>(8 - sh) * MAX_IP_MEM_BYTES;
    }

    // Keep CIP-PR safety margin so recover_IP (which uses indexed layers) is always safe.
    const size_t base_bytes = MAX_ITEM_MEM_BYTES;
    return std::max(base_bytes, peak_bytes);
}

/**
 * @brief Recover the Index Poiter (IP) layer at height h.
 *
 * This function reconstructs the IP layer at a specific height by recomputing
 * the Wagner algorithm layers from scratch. It's used in post-retrieval algorithms
 * where IP layers are not stored in memory during the forward pass. The proceeding
 * path is: L1 -> L2 -> ... -> L(h-1) -> L(h-1) with index -> IP(h)
 *
 * @param h The target layer height (1-9) for which to recover IP
 * @param seed The random seed used to generate initial layer 0
 * @param base Pointer to pre-allocated memory buffer for layer storage
 * @return Layer_IP The recovered Index Pair layer at height h
 *
 * Memory layout:
 * - Reuses the same memory buffer `base` for all intermediate layers
 * - Each merge operation overwrites the previous layer to save memory
 * - Final IP layer is stored in `out_IP` using the same base buffer
 */
inline Layer_IP recover_IP(int h, int seed, uint8_t *base)
{
    // Initialize all possible layer buffers (only used up to layer h)
    Layer0 L0 = init_layer<Item0>(base, MAX_LIST_SIZE * sizeof(Item0));
    Layer1 L1 = init_layer<Item1>(base, MAX_LIST_SIZE * sizeof(Item1));
    Layer2 L2 = init_layer<Item2>(base, MAX_LIST_SIZE * sizeof(Item2));
    Layer3 L3 = init_layer<Item3>(base, MAX_LIST_SIZE * sizeof(Item3));
    Layer4 L4 = init_layer<Item4>(base, MAX_LIST_SIZE * sizeof(Item4));
    Layer5 L5 = init_layer<Item5>(base, MAX_LIST_SIZE * sizeof(Item5));
    Layer6 L6 = init_layer<Item6>(base, MAX_LIST_SIZE * sizeof(Item6));
    Layer7 L7 = init_layer<Item7>(base, MAX_LIST_SIZE * sizeof(Item7));
    Layer8 L8 = init_layer<Item8>(base, MAX_LIST_SIZE * sizeof(Item8));
    Layer_IP out_IP = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);

    // Special case: h=1, generate L0 with indices and directly produce IP1
    if (h == 1)
    {
        Layer0_IDX L0_IDX = init_layer<Item0_IDX>(base, MAX_LIST_SIZE * sizeof(Item0_IDX));
        fill_layer0<Layer0_IDX>(L0_IDX, seed);
        set_index_batch(L0_IDX); // Set indices for IP tracking
        merge0_inplace_for_ip(L0_IDX, out_IP);
        clear_vec(L0_IDX);
        return out_IP;
    }

    // For h >= 2: start with non-indexed Layer0
    fill_layer0<Layer0>(L0, seed);
    merge0_inplace(L0, L1);
    clear_vec(L0); // Free memory immediately after use

    // h=2: convert L1 to indexed and generate IP2
    if (h == 2)
    {
        Layer1_IDX L1_IDX = expand_layer_to_idx_inplace<Item1, Item1_IDX>(L1);
        merge1_inplace_for_ip(L1_IDX, out_IP);
        clear_vec(L1_IDX);
        return out_IP;
    }

    // Continue merging up to layer h-1, then convert to indexed and generate IP
    merge1_inplace(L1, L2);
    clear_vec(L1);
    if (h == 3)
    {
        Layer2_IDX L2_IDX = expand_layer_to_idx_inplace<Item2, Item2_IDX>(L2);
        merge2_inplace_for_ip(L2_IDX, out_IP);
        clear_vec(L2_IDX);
        return out_IP;
    }

    merge2_inplace(L2, L3);
    clear_vec(L2);
    if (h == 4)
    {
        Layer3_IDX L3_IDX = expand_layer_to_idx_inplace<Item3, Item3_IDX>(L3);
        merge3_inplace_for_ip(L3_IDX, out_IP);
        clear_vec(L3_IDX);
        return out_IP;
    }

    merge3_inplace(L3, L4);
    clear_vec(L3);
    if (h == 5)
    {
        Layer4_IDX L4_IDX = expand_layer_to_idx_inplace<Item4, Item4_IDX>(L4);
        merge4_inplace_for_ip(L4_IDX, out_IP);
        clear_vec(L4_IDX);
        return out_IP;
    }

    merge4_inplace(L4, L5);
    clear_vec(L4);
    if (h == 6)
    {
        Layer5_IDX L5_IDX = expand_layer_to_idx_inplace<Item5, Item5_IDX>(L5);
        merge5_inplace_for_ip(L5_IDX, out_IP);
        clear_vec(L5_IDX);
        return out_IP;
    }

    merge5_inplace(L5, L6);
    clear_vec(L5);
    if (h == 7)
    {
        Layer6_IDX L6_IDX = expand_layer_to_idx_inplace<Item6, Item6_IDX>(L6);
        merge6_inplace_for_ip(L6_IDX, out_IP);
        clear_vec(L6_IDX);
        return out_IP;
    }

    merge6_inplace(L6, L7);
    clear_vec(L6);
    if (h == 8)
    {
        Layer7_IDX L7_IDX = expand_layer_to_idx_inplace<Item7, Item7_IDX>(L7);
        merge7_inplace_for_ip(L7_IDX, out_IP);
        clear_vec(L7_IDX);
        return out_IP;
    }

    // h=9: final layer
    merge7_inplace(L7, L8);
    clear_vec(L7);
    Layer8_IDX L8_IDX = expand_layer_to_idx_inplace<Item8, Item8_IDX>(L8);
    merge8_inplace_for_ip(L8_IDX, out_IP);
    clear_vec(L8_IDX);
    return out_IP;
}

/**
 * @brief Plain CIP (single-Chain with Index Pointer) algorithm - stores all IP layers in memory
 *
 * This is the baseline single-chain Wagner algorithm implementation that stores
 * all Index Pointer (IP) layers in memory during the forward pass. It provides
 * the fastest solution retrieval but uses the most memory.
 *
 * Algorithm flow:
 * 1. Forward pass: Generate layers L0 -> L1 -> ... -> L8 -> IP9
 *    - Each merge operation produces both the next layer and an IP array
 *    - IP arrays track which pairs from previous layer were merged
 * 2. Solution expansion: If solutions exist (IP9 non-empty), expand backwards
 *    - Use stored IP arrays to reconstruct the full solution tree
 *
 * @param seed Random seed for generating initial layer L0
 * @param base Optional pre-allocated memory buffer. If nullptr, allocates internally.
 * @return std::vector<Solution> All non-trivial solutions found
 *
 * Memory layout (total = MAX_ITEM_MEM_BYTES + 8 * MAX_IP_MEM_BYTES):
 * - [0, MAX_ITEM_MEM_BYTES): Layer storage (L0-L8, reused for each layer)
 * - [MAX_ITEM_MEM_BYTES, MAX_ITEM_MEM_BYTES + 8*MAX_IP_MEM_BYTES): IP1-IP8 storage
 * - IP9 overlays on the layer storage area after L8 is no longer needed
 *
 * Note: if pre-allocated memory buffer is provided (base != nullptr), it must
 * allocate at least `MAX_ITEM_MEM_BYTES + 8 * MAX_IP_MEM_BYTES` bytes.
 *
 */
std::vector<Solution> plain_cip(int seed, uint8_t *base = nullptr)
{
    // Total memory: one layer buffer + 8 IP arrays
    size_t total_mem = MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES * 8;
    bool own_base = false;
    if (!base)
    {
        base = static_cast<uint8_t *>(std::malloc(total_mem));
        own_base = true;
    }
    // Warning: Please ensure that the provided base memory is at least `total_mem` bytes, otherwise corrupted memory access may occur.

    // Initialize layer buffers (reuse same memory for all layers)
    Layer0_IDX L0 = init_layer<Item0_IDX>(base, MAX_LIST_SIZE * sizeof(Item0_IDX));
    Layer1_IDX L1 = init_layer<Item1_IDX>(base, MAX_LIST_SIZE * sizeof(Item1_IDX));
    Layer2_IDX L2 = init_layer<Item2_IDX>(base, MAX_LIST_SIZE * sizeof(Item2_IDX));
    Layer3_IDX L3 = init_layer<Item3_IDX>(base, MAX_LIST_SIZE * sizeof(Item3_IDX));
    Layer4_IDX L4 = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
    Layer5_IDX L5 = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
    Layer6_IDX L6 = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
    Layer7_IDX L7 = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
    Layer8_IDX L8 = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));

    // Initialize IP storage (each IP array gets its own dedicated memory)
    Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES); // Reuse layer buffer after L8
    Layer_IP IP8 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 0 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP7 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 1 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP6 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 2 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP5 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 3 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP4 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 4 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP3 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 5 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP2 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 6 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);
    Layer_IP IP1 = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES + 7 * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES);

    std::vector<Solution> solutions;
    IFV
    {
        std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
    }

    // Forward pass: Generate all layers and store all IP arrays
    // Pattern: fill/merge -> set_index -> clear previous layer -> repeat
    fill_layer0(L0, seed);
    set_index_batch(L0);            // Assign unique indices to L0 items for tracking
    merge0_ip_inplace(L0, L1, IP1); // Merge L0 pairs -> L1, store pairs in IP1
    set_index_batch(L1);
    clear_vec(L0); // Free L0 memory (reuse for next layer)

    merge1_ip_inplace(L1, L2, IP2);
    set_index_batch(L2);
    clear_vec(L1);

    merge2_ip_inplace(L2, L3, IP3);
    set_index_batch(L3);
    clear_vec(L2);

    merge3_ip_inplace(L3, L4, IP4);
    set_index_batch(L4);
    clear_vec(L3);

    merge4_ip_inplace(L4, L5, IP5);
    set_index_batch(L5);
    clear_vec(L4);

    merge5_ip_inplace(L5, L6, IP6);
    set_index_batch(L6);
    clear_vec(L5);

    merge6_ip_inplace(L6, L7, IP7);
    set_index_batch(L7);
    clear_vec(L6);

    merge7_ip_inplace(L7, L8, IP8);
    set_index_batch(L8);
    clear_vec(L7);

    merge8_inplace_for_ip(L8, IP9); // Final merge to get solutions
    clear_vec(L8);

    IFV
    {
        std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
        std::cout << "Layer 8 IP size: " << IP8.size() << std::endl;
        std::cout << "Layer 7 IP size: " << IP7.size() << std::endl;
        std::cout << "Layer 6 IP size: " << IP6.size() << std::endl;
        std::cout << "Layer 5 IP size: " << IP5.size() << std::endl;
        std::cout << "Layer 4 IP size: " << IP4.size() << std::endl;
        std::cout << "Layer 3 IP size: " << IP3.size() << std::endl;
        std::cout << "Layer 2 IP size: " << IP2.size() << std::endl;
        std::cout << "Layer 1 IP size: " << IP1.size() << std::endl;
    }

    // Backward pass: Expand solutions if any exist
    if (IP9.size() != 0)
    {
        // Expand from top to bottom: IP9 -> IP8 -> ... -> IP1
        // Each expansion step traces back one level in the solution tree
        expand_solutions(solutions, IP9);
        expand_solutions(solutions, IP8);
        expand_solutions(solutions, IP7);
        expand_solutions(solutions, IP6);
        expand_solutions(solutions, IP5);
        expand_solutions(solutions, IP4);
        expand_solutions(solutions, IP3);
        expand_solutions(solutions, IP2);
        expand_solutions(solutions, IP1);
        filter_trivial_solutions(solutions); // Remove solutions with repeated indices
    }

    if (own_base)
    {
        std::free(base);
    }
    return solutions;
}

/**
 * @brief Plain CIP with Post-Retrieval (CIP-PR) - minimizes memory by recomputing IP layers on demand
 *
 * This algorithm trades computation time for memory efficiency by using the "post-retrieval" strategy:
 * Instead of storing all IP layers during the forward pass, it only computes IP9 initially.
 * If solutions exist, it recovers each IP layer (IP8 down to IP1) on-demand by recomputing
 * the Wagner algorithm from scratch for each layer.
 *
 * Algorithm flow:
 * 1. Forward pass: Compute only IP9 (solutions at the top layer)
 *    - Internally recomputes L0 -> L1 -> ... -> L8 to get IP9
 * 2. Post-retrieval: If IP9 non-empty, recover each IP layer sequentially
 *    - For each h from 8 down to 1: recover_IP(h) recomputes L0 -> L(h-1) -> IP(h)
 *    - Each recovery pass reuses the same memory buffer
 *    - Solution expansion: Expand solutions backwards using recovered IP layer: IP(h)
 *
 * @param seed Random seed for generating initial layer L0
 * @param base Optional pre-allocated memory buffer. If nullptr, allocates internally.
 * @return std::vector<Solution> All non-trivial solutions found
 *
 * Memory layout (total = MAX_ITEM_MEM_BYTES):
 * - [0, MAX_ITEM_MEM_BYTES): Single reusable buffer for all layer computations
 * - Each recover_IP() call reuses this buffer for intermediate layers
 * - Final IP layers (IP9, IP8, ..., IP1) are computed one at a time and consumed immediately
 *
 * Note: if pre-allocated memory buffer is provided (base != nullptr), it must
 * allocate at least `MAX_ITEM_MEM_BYTES` bytes.
 */
std::vector<Solution> plain_cip_pr(int seed, uint8_t *base = nullptr)
{
    size_t total_mem = MAX_ITEM_MEM_BYTES;
    bool own_base = false;
    if (base == nullptr)
    {
        base = static_cast<uint8_t *>(std::malloc(total_mem));
        own_base = true;
    }
    // Warning: Please ensure that the provided base memory is at least `total_mem` bytes, otherwise corrupted memory access may occur.
    IFV
    {
        std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
    }
    std::vector<Solution> solutions;

    // Step 1: Compute IP9 (top-level solutions) via recover_IP
    // This internally computes L0 -> L1 -> ... -> L8 -> IP9
    Layer_IP IP9 = recover_IP(9, seed, base);
    IFV { std::cout << "Layer " << 9 << " IP size: " << IP9.size() << std::endl; }

    // Step 2: If solutions exist, recover all IP layers and expand backwards
    if (IP9.size() != 0)
    {
        // Expand IP9 first to get layer-8 indices
        expand_solutions(solutions, IP9);

        // Recover and expand each IP layer from h=8 down to h=1
        // Each recover_IP call recomputes L0 -> L(h-1) -> IPh using the same buffer
        for (int h = 8; h >= 1; --h)
        {
            Layer_IP IPh = recover_IP(h, seed, base); // Recompute IPh on-demand
            IFV { std::cout << "Layer " << h << " IP size: " << IPh.size() << std::endl; }
            expand_solutions(solutions, IPh); // Expand solutions using IPh
        }
        IFV
        {
            if (solutions.size() > 1024)
            {
                std::cout << "Warning: Large number of solutions found: " << solutions.size() << std::endl;
            }
        }
        filter_trivial_solutions(solutions); // Remove solutions with repeated indices
    }
    if (own_base)
    {
        std::free(base);
    }
    return solutions;
}

/**
 * @brief Advanced CIP with Post-Retrieval (Advanced CIP-PR) - memory layout optimization
 *
 * This algorithm uses an advanced strategy that stores some IP layers while recovering others.
 * The switching height controls how many IP layers are kept:
 * - Layers above switch_h: stored during the forward pass (IP_{switch_h+1}..IP8)
 * - Layers at/below switch_h: recovered on demand during the backward pass
 *
 * Algorithm flow:
 * 1. Forward pass (non-indexed) until layer switch_h
 *    - Uses Item* types (no index field) to save memory
 * 2. Convert the layer at height switch_h to indexed form
 * 3. Forward pass (indexed) with IP storage for higher layers
 *    - Store IP_{switch_h+1}..IP8 in memory, compute IP9
 * 4. Backward pass:
 *    - Expand stored IPs (9 down to switch_h+1)
 *    - Recover IP_{switch_h}..IP1 on demand using recover_IP
 *
 *
 * @param seed Random seed for generating initial layer L0
 * @param switch_h Switching height (0 => plain CIP, 8 => pure PR)
 * @param base Optional pre-allocated memory buffer. If nullptr, allocates internally.
 * @return std::vector<Solution> All non-trivial solutions found
 *
 * Memory sizing: use calc_apr_mem_bytes(switch_h) to provision a buffer big
 * enough for the active layer plus the stored IP arrays.
 */
std::vector<Solution> advanced_cip_pr(int seed, int switch_h, uint8_t *base)
{
    // Normalize switch height and reuse specialized paths on the extremes.
    if (switch_h <= 0)
        return plain_cip(seed, base);
    if (switch_h >= 8)
        return plain_cip_pr(seed, base);

    const size_t total_mem = calc_apr_mem_bytes(switch_h);
    bool own_base = false;
    if (base == nullptr)
    {
        base = static_cast<uint8_t *>(std::malloc(total_mem));
        own_base = true;
    }
    IFV
    {
        std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << " (switch_h=" << switch_h << ")" << std::endl;
    }

    uint8_t *base_end = base + total_mem;

    // Layer buffers (non-indexed and indexed views share the same base)
    Layer0 L0 = init_layer<Item0>(base, total_mem);
    Layer1 L1 = init_layer<Item1>(base, total_mem);
    Layer2 L2 = init_layer<Item2>(base, total_mem);
    Layer3 L3 = init_layer<Item3>(base, total_mem);
    Layer4 L4 = init_layer<Item4>(base, total_mem);
    Layer5 L5 = init_layer<Item5>(base, total_mem);
    Layer6 L6 = init_layer<Item6>(base, total_mem);
    Layer7 L7 = init_layer<Item7>(base, total_mem);
    Layer8 L8 = init_layer<Item8>(base, total_mem);

    Layer0_IDX L0_IDX = init_layer<Item0_IDX>(base, total_mem);
    Layer1_IDX L1_IDX = init_layer<Item1_IDX>(base, total_mem);
    Layer2_IDX L2_IDX = init_layer<Item2_IDX>(base, total_mem);
    Layer3_IDX L3_IDX = init_layer<Item3_IDX>(base, total_mem);
    Layer4_IDX L4_IDX = init_layer<Item4_IDX>(base, total_mem);
    Layer5_IDX L5_IDX = init_layer<Item5_IDX>(base, total_mem);
    Layer6_IDX L6_IDX = init_layer<Item6_IDX>(base, total_mem);
    Layer7_IDX L7_IDX = init_layer<Item7_IDX>(base, total_mem);
    Layer8_IDX L8_IDX = init_layer<Item8_IDX>(base, total_mem);

    // IP storage (only store layers above the switching height)
    const int stored_ip_cnt = 8 - switch_h;
    std::vector<Layer_IP> stored_ips;
    stored_ips.reserve(stored_ip_cnt);
    for (int i = 0; i < stored_ip_cnt; ++i)
    {
        stored_ips.push_back(init_layer<Item_IP>(base_end - static_cast<size_t>(i + 1) * MAX_IP_MEM_BYTES, MAX_IP_MEM_BYTES));
    }
    auto get_stored_ip = [&](int ip_level) -> Layer_IP & {
        // ip_level in [switch_h+1, 8]
        return stored_ips[ip_level - (switch_h + 1)];
    };
    Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES); // Reuse layer buffer

    // ------------------------------------------------------------------
    // Forward pass: XOR-only until switch_h, then indexed with IP store.
    // ------------------------------------------------------------------
    fill_layer0(L0, seed);
    if (switch_h > 0)
    {
        merge0_inplace(L0, L1);
        clear_vec(L0);
    }
    if (switch_h > 1)
    {
        merge1_inplace(L1, L2);
        clear_vec(L1);
    }
    if (switch_h > 2)
    {
        merge2_inplace(L2, L3);
        clear_vec(L2);
    }
    if (switch_h > 3)
    {
        merge3_inplace(L3, L4);
        clear_vec(L3);
    }
    if (switch_h > 4)
    {
        merge4_inplace(L4, L5);
        clear_vec(L4);
    }
    if (switch_h > 5)
    {
        merge5_inplace(L5, L6);
        clear_vec(L5);
    }
    if (switch_h > 6)
    {
        merge6_inplace(L6, L7);
        clear_vec(L6);
    }
    if (switch_h > 7)
    {
        merge7_inplace(L7, L8);
        clear_vec(L7);
    }

    // Convert the current layer at height switch_h to indexed form
    switch (switch_h)
    {
    case 1:
        L1_IDX = expand_layer_to_idx_inplace<Item1, Item1_IDX>(L1);
        set_index_batch(L1_IDX);
        break;
    case 2:
        L2_IDX = expand_layer_to_idx_inplace<Item2, Item2_IDX>(L2);
        set_index_batch(L2_IDX);
        break;
    case 3:
        L3_IDX = expand_layer_to_idx_inplace<Item3, Item3_IDX>(L3);
        set_index_batch(L3_IDX);
        break;
    case 4:
        L4_IDX = expand_layer_to_idx_inplace<Item4, Item4_IDX>(L4);
        set_index_batch(L4_IDX);
        break;
    case 5:
        L5_IDX = expand_layer_to_idx_inplace<Item5, Item5_IDX>(L5);
        set_index_batch(L5_IDX);
        break;
    case 6:
        L6_IDX = expand_layer_to_idx_inplace<Item6, Item6_IDX>(L6);
        set_index_batch(L6_IDX);
        break;
    case 7:
        L7_IDX = expand_layer_to_idx_inplace<Item7, Item7_IDX>(L7);
        set_index_batch(L7_IDX);
        break;
    default:
        break;
    }

    // Indexed merges with IP storage from layer switch_h to 7
    auto merge_with_ip = [&](int lvl) {
        switch (lvl)
        {
        case 1:
            merge1_ip_inplace(L1_IDX, L2_IDX, get_stored_ip(2));
            clear_vec(L1_IDX);
            set_index_batch(L2_IDX);
            break;
        case 2:
            merge2_ip_inplace(L2_IDX, L3_IDX, get_stored_ip(3));
            clear_vec(L2_IDX);
            set_index_batch(L3_IDX);
            break;
        case 3:
            merge3_ip_inplace(L3_IDX, L4_IDX, get_stored_ip(4));
            clear_vec(L3_IDX);
            set_index_batch(L4_IDX);
            break;
        case 4:
            merge4_ip_inplace(L4_IDX, L5_IDX, get_stored_ip(5));
            clear_vec(L4_IDX);
            set_index_batch(L5_IDX);
            break;
        case 5:
            merge5_ip_inplace(L5_IDX, L6_IDX, get_stored_ip(6));
            clear_vec(L5_IDX);
            set_index_batch(L6_IDX);
            break;
        case 6:
            merge6_ip_inplace(L6_IDX, L7_IDX, get_stored_ip(7));
            clear_vec(L6_IDX);
            set_index_batch(L7_IDX);
            break;
        case 7:
            merge7_ip_inplace(L7_IDX, L8_IDX, get_stored_ip(8));
            clear_vec(L7_IDX);
            set_index_batch(L8_IDX);
            break;
        default:
            break;
        }
    };

    for (int lvl = switch_h; lvl <= 7; ++lvl)
    {
        merge_with_ip(lvl);
    }

    merge8_inplace_for_ip(L8_IDX, IP9); // Generate IP9 (solutions)
    clear_vec(L8_IDX);

    IFV
    {
        std::cout << "Layer 9 IP size: " << IP9.size() << std::endl;
        for (int lvl = 8; lvl >= switch_h + 1; --lvl)
        {
            std::cout << "Layer " << lvl << " IP size: " << get_stored_ip(lvl).size() << std::endl;
        }
    }

    std::vector<Solution> solutions;

    // Backward pass: expand stored IPs, then recover the rest on demand.
    if (IP9.size() != 0)
    {
        expand_solutions(solutions, IP9);
        for (int lvl = 8; lvl >= switch_h + 1; --lvl)
        {
            expand_solutions(solutions, get_stored_ip(lvl));
        }
        for (int h = switch_h; h >= 1; --h)
        {
            Layer_IP IPh = recover_IP(h, seed, base); // Recompute IPh on-demand
            IFV { std::cout << "Layer " << h << " IP size: " << IPh.size() << std::endl; }
            expand_solutions(solutions, IPh);
        }
        filter_trivial_solutions(solutions); // Remove solutions with repeated indices
    }

    if (own_base)
    {
        std::free(base);
    }
    return solutions;
}

/**
 * @brief CIP with External Memory (CIP-EM) - stores IP layers to disk for minimal memory usage
 *
 * This algorithm minimizes memory usage by storing all Index Pointer (IP) layers to disk
 * instead of keeping them in RAM. It writes IP arrays sequentially to a file during the
 * forward pass, then reads them back during the backward pass for solution expansion.
 *
 * Algorithm flow:
 * 1. Forward pass: Generate layers L0 -> L1 -> ... -> L8 -> IP9
 *    - Each merge operation produces both the next layer and an IP array
 *    - IP arrays (IP1-IP8) are immediately written to disk via streaming writer
 *    - Only IP9 (solutions) is kept in memory
 * 2. Backward pass: If solutions exist (IP9 non-empty), expand using disk storage
 *    - Open file reader and extract IP Items (IP8 down to IP1) from disk on-demand
 *    - Only 2^(k +1) reads is needed to expand solutions
 *
 * @param seed Random seed for generating initial layer L0
 * @param em_path File path for storing external memory (IP layers)
 * @param base Optional pre-allocated memory buffer. If nullptr, allocates internally.
 * @return std::vector<Solution> All non-trivial solutions found
 *
 * Memory layout (total = MAX_ITEM_MEM_BYTES):
 * - [0, MAX_ITEM_MEM_BYTES): Layer storage (L0-L8, reused for each layer)
 * - IP9 reuses the layer buffer after L8 is consumed
 * - IP1-IP8 are stored on disk (not in RAM)
 *
 * Disk storage format:
 * - IP arrays are written sequentially using IPDiskWriter (streaming write)
 * - IPDiskManifest tracks offset and count for each IP layer
 * - Total disk usage: ~sum(IP1.size() to IP8.size()) * sizeof(Item_IP)
 *
 * Memory efficiency:
 * - Peak memory: ~MAX_ITEM_MEM_BYTES (single layer buffer)
 * - Same as plain_cip_pr but trades computation for I/O
 * - Disk I/O overhead: sequential write (forward) + sequential read (backward), the latter being more negeligible.
 *
 * Note: if pre-allocated memory buffer is provided (base != nullptr), it must
 * allocate at least `MAX_ITEM_MEM_BYTES` bytes.
 */
std::vector<Solution> cip_em(int seed, const std::string &em_path, uint8_t *base = nullptr)
{
    const size_t total_mem = MAX_ITEM_MEM_BYTES;
    bool own_base = false;
    if (base == nullptr)
    {
        base = static_cast<uint8_t *>(std::malloc(total_mem));
        own_base = true;
    }
    IFV
    {
        std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
    }
    // Warning: Please ensure that the provided base memory is at least `total_mem` bytes, otherwise corrupted memory access may occur.

    // Initialize layer buffers (reuse same memory for all layers)
    Layer0_IDX L0 = init_layer<Item0_IDX>(base, MAX_LIST_SIZE * sizeof(Item0_IDX));
    Layer1_IDX L1 = init_layer<Item1_IDX>(base, MAX_LIST_SIZE * sizeof(Item1_IDX));
    Layer2_IDX L2 = init_layer<Item2_IDX>(base, MAX_LIST_SIZE * sizeof(Item2_IDX));
    Layer3_IDX L3 = init_layer<Item3_IDX>(base, MAX_LIST_SIZE * sizeof(Item3_IDX));
    Layer4_IDX L4 = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
    Layer5_IDX L5 = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
    Layer6_IDX L6 = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
    Layer7_IDX L7 = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
    Layer8_IDX L8 = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
    Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES); // Reuse layer buffer

    // Initialize disk I/O structures
    // Initialize disk I/O structures
    IPDiskManifest manifest; // Tracks offset and count for each IP layer on disk
    IPDiskWriter writer;     // Streaming writer for IP arrays

    if (!writer.open(em_path.c_str()))
    {
        std::cerr << "Failed to open EM file: " << em_path << std::endl;
        std::free(base);
        return std::vector<Solution>();
    }

    // Forward pass: Generate all layers and stream IP arrays to disk
    // Pattern: fill/merge -> record IP metadata -> set_index -> clear previous layer
    manifest.ip[0].offset = 0;
    fill_layer0<Layer0_IDX>(L0, seed);
    set_index_batch(L0);                  // Assign unique indices for tracking
    merge0_em_ip_inplace(L0, L1, writer); // Merge and stream IP1 to disk
    manifest.ip[0].count = L1.size();
    manifest.ip[1].offset = writer.get_current_offset();
    set_index_batch(L1);
    clear_vec(L0);
    IFV { std::cout << "Layer 1 size: " << L1.size() << std::endl; }

    merge1_em_ip_inplace(L1, L2, writer); // Stream IP2 to disk
    manifest.ip[1].count = L2.size();
    manifest.ip[2].offset = writer.get_current_offset();
    set_index_batch(L2);
    clear_vec(L1);
    IFV { std::cout << "Layer 2 size: " << L2.size() << std::endl; }

    merge2_em_ip_inplace(L2, L3, writer); // Stream IP3 to disk
    manifest.ip[2].count = L3.size();
    manifest.ip[3].offset = writer.get_current_offset();
    set_index_batch(L3);
    clear_vec(L2);
    IFV { std::cout << "Layer 3 size: " << L3.size() << std::endl; }

    merge3_em_ip_inplace(L3, L4, writer); // Stream IP4 to disk
    manifest.ip[3].count = L4.size();
    manifest.ip[4].offset = writer.get_current_offset();
    set_index_batch(L4);
    clear_vec(L3);
    IFV { std::cout << "Layer 4 size: " << L4.size() << std::endl; }

    merge4_em_ip_inplace(L4, L5, writer); // Stream IP5 to disk
    manifest.ip[4].count = L5.size();
    manifest.ip[5].offset = writer.get_current_offset();
    set_index_batch(L5);
    clear_vec(L4);
    IFV { std::cout << "Layer 5 size: " << L5.size() << std::endl; }

    merge5_em_ip_inplace(L5, L6, writer); // Stream IP6 to disk
    manifest.ip[5].count = L6.size();
    manifest.ip[6].offset = writer.get_current_offset();
    set_index_batch(L6);
    clear_vec(L5);
    IFV { std::cout << "Layer 6 size: " << L6.size() << std::endl; }

    merge6_em_ip_inplace(L6, L7, writer); // Stream IP7 to disk
    manifest.ip[6].count = L7.size();
    manifest.ip[7].offset = writer.get_current_offset();
    set_index_batch(L7);
    clear_vec(L6);
    IFV { std::cout << "Layer 7 size: " << L7.size() << std::endl; }

    merge7_em_ip_inplace(L7, L8, writer); // Stream IP8 to disk
    manifest.ip[7].count = L8.size();
    set_index_batch(L8);
    clear_vec(L7);
    IFV { std::cout << "Layer 8 size: " << L8.size() << std::endl; }

    merge8_inplace_for_ip(L8, IP9); // Generate IP9 (solutions) in memory

    std::vector<Solution> solutions;
    IPDiskReader reader;
    writer.close(); // Flush all data to disk

    // Backward pass: Expand solutions if any exist
    if (IP9.size() != 0)
    {
        if (!reader.open(em_path.c_str()))
        {
            std::cerr << "Cannot open EM file for reading\n";
            if (own_base)
            {
                std::free(base);
            }
            return solutions;
        }

        // Expand IP9 first (in memory)
        expand_solutions(solutions, IP9);

        // Read and expand IP8 down to IP1 from disk
        // Each IP layer is read sequentially from disk using manifest metadata
        for (int i = 7; i >= 0; --i)
        {
            expand_solutions_from_file(solutions, reader, manifest.ip[i]);
        }
        filter_trivial_solutions(solutions); // Remove solutions with repeated indices
    }

    if (own_base)
    {
        std::free(base);
    }
    reader.close();
    return solutions;
}

/**
 * @brief CIP with External Memory and Extra IP Cache - reduces disk I/O by buffering one IP array
 *
 * This is a variant of CIP-EM that allocates an extra IP array buffer in memory to reduce
 * the number of small disk writes. Instead of streaming IP data directly during merge operations,
 * it first stores each IP array in memory, then writes the complete array to disk.
 *
 * Algorithm flow:
 * 1. Forward pass: Generate layers L0 -> L1 -> ... -> L8 -> IP9
 *    - Each merge produces an IP array stored temporarily in IP_cache
 *    - IP_cache is then written to disk as a complete array
 *    - IP9 (solutions) is kept in memory
 * 2. Backward pass: Same as cip_em - read IP arrays from disk for solution expansion
 *
 * @param seed Random seed for generating initial layer L0
 * @param em_path File path for storing external memory (IP layers)
 * @param base Optional pre-allocated memory buffer. If nullptr, allocates internally.
 * @return std::vector<Solution> All non-trivial solutions found
 *
 * Memory layout (total = MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES):
 * - [0, MAX_ITEM_MEM_BYTES): Layer storage (L0-L8, reused for each layer)
 * - [MAX_ITEM_MEM_BYTES, MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES): IP_cache buffer
 * - IP9 reuses the layer buffer after L8 is consumed
 *
 * I/O pattern differences from cip_em:
 * - cip_em: Streaming writes (many small writes during merge)
 * - cip_em_extra_ip_cache: Buffered writes (one large write per IP layer)
 * - In practice: minimal performance difference due to OS-level buffering
 *
 * Memory efficiency:
 * - Peak memory: ~MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES
 * - Slightly more memory than cip_em for potential I/O optimization
 *
 * Note: if pre-allocated memory buffer is provided (base != nullptr), it must
 * allocate at least `MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES` bytes.
 */
std::vector<Solution> cip_em_extra_ip_cache(int seed, const std::string &em_path, uint8_t *base = nullptr)
{
    const size_t total_mem = MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES;
    bool own_base = false;
    if (base == nullptr)
    {
        base = static_cast<uint8_t *>(std::malloc(total_mem));
        own_base = true;
    }
    IFV
    {
        std::cout << "Total memory allocated (MB): " << total_mem / (1024 * 1024) << std::endl;
    }
    // Warning: Please ensure that the provided base memory is at least `total_mem` bytes, otherwise corrupted memory access may occur.

    // Initialize layer buffers (reuse same memory for all layers)
    Layer0_IDX L0 = init_layer<Item0_IDX>(base, MAX_LIST_SIZE * sizeof(Item0_IDX));
    Layer1_IDX L1 = init_layer<Item1_IDX>(base, MAX_LIST_SIZE * sizeof(Item1_IDX));
    Layer2_IDX L2 = init_layer<Item2_IDX>(base, MAX_LIST_SIZE * sizeof(Item2_IDX));
    Layer3_IDX L3 = init_layer<Item3_IDX>(base, MAX_LIST_SIZE * sizeof(Item3_IDX));
    Layer4_IDX L4 = init_layer<Item4_IDX>(base, MAX_LIST_SIZE * sizeof(Item4_IDX));
    Layer5_IDX L5 = init_layer<Item5_IDX>(base, MAX_LIST_SIZE * sizeof(Item5_IDX));
    Layer6_IDX L6 = init_layer<Item6_IDX>(base, MAX_LIST_SIZE * sizeof(Item6_IDX));
    Layer7_IDX L7 = init_layer<Item7_IDX>(base, MAX_LIST_SIZE * sizeof(Item7_IDX));
    Layer8_IDX L8 = init_layer<Item8_IDX>(base, MAX_LIST_SIZE * sizeof(Item8_IDX));
    Layer_IP IP_cache = init_layer<Item_IP>(base + MAX_ITEM_MEM_BYTES, MAX_IP_MEM_BYTES); // Extra buffer for IP
    Layer_IP IP9 = init_layer<Item_IP>(base, MAX_IP_MEM_BYTES);                           // Reuse layer buffer

    // Initialize disk I/O structures
    IPDiskManifest manifest; // Tracks offset and count for each IP layer on disk
    IPDiskWriter writer;     // Writer for complete IP arrays

    if (!writer.open(em_path.c_str()))
    {
        std::cerr << "Failed to open EM file: " << em_path << std::endl;
        std::free(base);
        return std::vector<Solution>();
    }

    // Forward pass: Generate layers and buffer IP arrays before writing to disk
    // Pattern: fill/merge -> buffer IP in cache -> write complete array -> clear cache
    fill_layer0(L0, seed);
    set_index_batch(L0);
    merge0_ip_inplace(L0, L1, IP_cache); // Store IP1 in cache
    manifest.ip[0].count = L1.size();
    manifest.ip[0].offset = writer.append_layer(IP_cache.data(), IP_cache.size()); // Write complete IP1
    set_index_batch(L1);
    clear_vec(L0);
    clear_vec(IP_cache); // Clear cache for next IP
    IFV { std::cout << "Layer 1 size: " << L1.size() << std::endl; }

    merge1_ip_inplace(L1, L2, IP_cache); // Store IP2 in cache
    manifest.ip[1].count = IP_cache.size();
    manifest.ip[1].offset = writer.append_layer(IP_cache.data(), IP_cache.size()); // Write complete IP2
    set_index_batch(L2);
    clear_vec(L1);
    clear_vec(IP_cache);
    IFV { std::cout << "Layer 2 size: " << L2.size() << std::endl; }

    merge2_ip_inplace(L2, L3, IP_cache); // Store IP3 in cache
    manifest.ip[2].count = L3.size();
    manifest.ip[2].offset = writer.append_layer(IP_cache.data(), IP_cache.size()); // Write complete IP3
    set_index_batch(L3);
    clear_vec(L2);
    clear_vec(IP_cache);
    IFV { std::cout << "Layer 3 size: " << L3.size() << std::endl; }

    merge3_ip_inplace(L3, L4, IP_cache); // Store IP4 in cache
    manifest.ip[3].count = L4.size();
    manifest.ip[3].offset = writer.append_layer(IP_cache.data(), IP_cache.size()); // Write complete IP4
    set_index_batch(L4);
    clear_vec(L3);
    clear_vec(IP_cache);
    IFV { std::cout << "Layer 4 size: " << L4.size() << std::endl; }

    merge4_ip_inplace(L4, L5, IP_cache); // Store IP5 in cache
    manifest.ip[4].count = L5.size();
    manifest.ip[4].offset = writer.append_layer(IP_cache.data(), IP_cache.size()); // Write complete IP5
    set_index_batch(L5);
    clear_vec(L4);
    clear_vec(IP_cache);
    IFV { std::cout << "Layer 5 size: " << L5.size() << std::endl; }

    merge5_ip_inplace(L5, L6, IP_cache); // Store IP6 in cache
    manifest.ip[5].count = L6.size();
    manifest.ip[5].offset = writer.append_layer(IP_cache.data(), IP_cache.size()); // Write complete IP6
    set_index_batch(L6);
    clear_vec(L5);
    clear_vec(IP_cache);
    IFV { std::cout << "Layer 6 size: " << L6.size() << std::endl; }

    merge6_ip_inplace(L6, L7, IP_cache); // Store IP7 in cache
    manifest.ip[6].count = L7.size();
    manifest.ip[6].offset = writer.append_layer(IP_cache.data(), IP_cache.size()); // Write complete IP7
    set_index_batch(L7);
    clear_vec(L6);
    clear_vec(IP_cache);
    IFV { std::cout << "Layer 7 size: " << L7.size() << std::endl; }

    merge7_ip_inplace(L7, L8, IP_cache); // Store IP8 in cache
    manifest.ip[7].count = L8.size();
    manifest.ip[7].offset = writer.append_layer(IP_cache.data(), IP_cache.size()); // Write complete IP8
    set_index_batch(L8);
    clear_vec(L7);
    clear_vec(IP_cache);
    IFV { std::cout << "Layer 8 size: " << L8.size() << std::endl; }

    merge8_inplace_for_ip(L8, IP9); // Generate IP9 (solutions) in memory

    // Backward pass: Expand solutions from disk
    std::vector<Solution> solutions;
    IPDiskReader reader;
    writer.close(); // Flush all data to disk

    if (IP9.size() != 0)
    {
        if (!reader.open(em_path.c_str()))
        {
            std::cerr << "Cannot open EM file for reading\n";
            if (own_base)
            {
                std::free(base);
            }
            return solutions;
        }
        // Start with IP9 (in memory)
        expand_solutions(solutions, IP9);
        for (int i = 7; i >= 0; --i)
        {
            expand_solutions_from_file(solutions, reader, manifest.ip[i]);
        }
        filter_trivial_solutions(solutions);
    }
    if (own_base)
    {
        std::free(base);
    }

    reader.close();
    return solutions;
}
