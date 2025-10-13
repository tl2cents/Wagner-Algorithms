#include "eq_core.h"
#include "zcash_blake.h"

// verbose-guarded logging helper
#define IFV if (g_verbose)

SortAlgo g_sort_algo = SortAlgo::KXSORT;
bool g_verbose = false;

// ---------- L0 filling ----------
static inline void fill_blake_layer0(Layer0_IDX& L0, const uint8_t* header,
                                     size_t header_len,
                                     const uint8_t nonce[32]) {
    static constexpr uint32_t HALF = 1u << 20;  // 1,048,576
    static constexpr uint32_t FULL = HALF * 2;  // 2,097,152

    L0.resize(FULL);
    ZcashEquihashHasher H;
    H.init_midstate(header, header_len, nonce, /*n=*/200, /*k=*/9);

    uint8_t out[ZcashEquihashHasher::OUT_LEN];
    for (uint32_t i = 0; i < HALF; ++i) {
        H.hash_index(i, out);
        std::memcpy(L0[2 * i + 0].XOR, out + 0, 25);
        std::memcpy(L0[2 * i + 1].XOR, out + 25, 25);
    }
    set_index_batch(L0);
}

// synthesize a deterministic header/nonce from 'seed'
inline void fill_layer0_from_seed(Layer0_IDX& L0, int seed) {
    // Match Tromp's implementation exactly:
    // headernonce is 140 bytes with nonce at position 108 (word 27)
    uint8_t headernonce[140];
    std::memset(headernonce, 0, sizeof(headernonce));

    // Put nonce at position 108 (as u32 word 27) in little-endian format
    // This matches: ((u32 *)headernonce)[27] = htole32(nonce);
    uint32_t* nonce_ptr = (uint32_t*)(headernonce + 108);
    *nonce_ptr = seed;  // Already little-endian on x86

    // allocate L0
    static constexpr uint32_t HALF = 1u << 20;  // 1,048,576
    static constexpr uint32_t FULL = HALF * 2;  // 2,097,152
    L0.resize(FULL);

    // Build midstate with headernonce (no separate nonce array)
    // We need to match Tromp's setheader which only takes the 140-byte
    // headernonce
    uint8_t dummy_nonce[32];
    std::memset(dummy_nonce, 0, sizeof(dummy_nonce));

    ZcashEquihashHasher H;
    H.init_midstate(headernonce, sizeof(headernonce), dummy_nonce, /*n=*/200,
                    /*k=*/9);

    // Process indices in blocks of 4 (NBLAKES=4)
    uint8_t out[4][ZcashEquihashHasher::OUT_LEN];
    uint32_t i = 0;
    for (; i + 3 < HALF; i += 4) {
        H.hash_index(i + 0, out[0]);
        H.hash_index(i + 1, out[1]);
        H.hash_index(i + 2, out[2]);
        H.hash_index(i + 3, out[3]);

        // Store 8 leaves (2 per index)
        for (int lane = 0; lane < 4; ++lane) {
            const int base = (2 * (i + lane));
            std::memcpy(L0[base + 0].XOR, out[lane] + 0, 25);
            std::memcpy(L0[base + 1].XOR, out[lane] + 25, 25);
        }
    }
    // tail
    for (; i < HALF; ++i) {
        H.hash_index(i, out[0]);
        std::memcpy(L0[2 * i + 0].XOR, out[0] + 0, 25);
        std::memcpy(L0[2 * i + 1].XOR, out[0] + 25, 25);
    }
    set_index_batch(L0);
}

// ---------- CIP forward ----------
struct CIPParams {
    int n_bits = N_BITS, ell_bits = ELL_BITS, k = K_ROUNDS;
};

static inline void run_cip_forward_seed(
    int seed, Layer0_IDX& L0, Layer1_IDX& L1, Layer2_IDX& L2, Layer3_IDX& L3,
    Layer4_IDX& L4, Layer5_IDX& L5, Layer6_IDX& L6, Layer7_IDX& L7,
    Layer8_IDX& L8, Layer9_IDX& L9, Layer_IP& IP1, Layer_IP& IP2, Layer_IP& IP3,
    Layer_IP& IP4, Layer_IP& IP5, Layer_IP& IP6, Layer_IP& IP7, Layer_IP& IP8,
    Layer_IP& IP9) {
    fill_layer0_from_seed(L0, seed);
    IFV {
        std::cout << "========================================================="
                     "======================\n"
                  << "CIP Forward Pass - Seed: " << seed << "\n"
                  << "========================================================="
                     "======================\n"
                  << "Layer 0 size: " << L0.size() << "\n";
        print_layer_debug("Layer 0 (first 3 items)", L0, 3);
    }

    clear_vec(L1);
    clear_vec(IP1);
    merge0_ip_inplace(L0, L1, IP1);
    set_index_batch(L1);
    IFV {
        std::cout << "Layer 1 size: " << L1.size() << "\n";
        print_layer_debug("Layer 1 (first 3 items)", L1, 3);
    }

    clear_vec(L2);
    clear_vec(IP2);
    merge1_ip_inplace(L1, L2, IP2);
    set_index_batch(L2);
    IFV {
        std::cout << "Layer 2 size: " << L2.size() << "\n";
        print_layer_debug("Layer 2 (first 3 items)", L2, 3);
    }

    clear_vec(L3);
    clear_vec(IP3);
    merge2_ip_inplace(L2, L3, IP3);
    set_index_batch(L3);
    IFV {
        std::cout << "Layer 3 size: " << L3.size() << "\n";
        print_layer_debug("Layer 3 (first 3 items)", L3, 3);
    }

    clear_vec(L4);
    clear_vec(IP4);
    merge3_ip_inplace(L3, L4, IP4);
    set_index_batch(L4);
    IFV {
        std::cout << "Layer 4 size: " << L4.size() << "\n";
        print_layer_debug("Layer 4 (first 3 items)", L4, 3);
    }

    clear_vec(L5);
    clear_vec(IP5);
    merge4_ip_inplace(L4, L5, IP5);
    set_index_batch(L5);
    IFV {
        std::cout << "Layer 5 size: " << L5.size() << "\n";
        print_layer_debug("Layer 5 (first 3 items)", L5, 3);
    }

    clear_vec(L6);
    clear_vec(IP6);
    merge5_ip_inplace(L5, L6, IP6);
    set_index_batch(L6);
    IFV {
        std::cout << "Layer 6 size: " << L6.size() << "\n";
        print_layer_debug("Layer 6 (first 3 items)", L6, 3);
    }

    clear_vec(L7);
    clear_vec(IP7);
    merge6_ip_inplace(L6, L7, IP7);
    set_index_batch(L7);
    IFV {
        std::cout << "Layer 7 size: " << L7.size() << "\n";
        print_layer_debug("Layer 7 (first 3 items)", L7, 3);
    }

    clear_vec(L8);
    clear_vec(IP8);
    merge7_ip_inplace(L7, L8, IP8);
    set_index_batch(L8);
    IFV {
        std::cout << "Layer 8 size: " << L8.size() << "\n";
        print_layer_debug("Layer 8 (first 3 items)", L8, 3);
    }

    clear_vec(L9);
    clear_vec(IP9);
    merge8_ip_inplace(L8, L9, IP9);
    set_index_batch(L9);
    IFV {
        std::cout << "Layer 9 size: " << L9.size()
                  << " (final collision candidates)\n";
        print_layer_debug("Layer 9 (first 5 items)", L9, 5);
        std::cout << "[CIP] Forward finished. IP sizes: IP1=" << IP1.size()
                  << " IP2=" << IP2.size() << " IP3=" << IP3.size()
                  << " IP4=" << IP4.size() << " IP5=" << IP5.size()
                  << " IP6=" << IP6.size() << " IP7=" << IP7.size()
                  << " IP8=" << IP8.size() << " IP9=" << IP9.size() << "\n";
    }
}

// ---------- Expand (CIP / PR / EM) ----------
static inline void cip_expand_ip9_resident(
    const Layer_IP& IP1, const Layer_IP& IP2, const Layer_IP& IP3,
    const Layer_IP& IP4, const Layer_IP& IP5, const Layer_IP& IP6,
    const Layer_IP& IP7, const Layer_IP& IP8, const Layer_IP& IP9,
    std::vector<std::vector<size_t>>& expanded) {
    IFV {
        std::cout << "---------------------------------------------------------"
                     "----------------------\n"
                  << "[CIP-Expand] Expanding IP9 via resident IP1..IP8...\n";
    }

    expanded.clear();
    expanded.reserve(IP9.size());
    auto expand_side = [&](size_t idx, const Layer_IP* const IPs[8],
                           std::vector<size_t>& out) {
        std::vector<size_t> cur = {idx};
        for (int level = 7; level >= 0; --level) {
            const Layer_IP& L = *IPs[level];
            std::vector<size_t> nxt;
            nxt.reserve(cur.size() * 2);
            for (size_t id : cur) {
                if (id >= L.size()) continue;
                const auto& node = L[id];
                nxt.push_back(get_index_from_bytes(node.index_pointer_left));
                nxt.push_back(get_index_from_bytes(node.index_pointer_right));
            }
            cur.swap(nxt);
        }
        out.insert(out.end(), cur.begin(), cur.end());
    };

    const Layer_IP* IPs[8] = {&IP1, &IP2, &IP3, &IP4, &IP5, &IP6, &IP7, &IP8};
    for (size_t i = 0; i < IP9.size(); ++i) {
        std::vector<size_t> indices;
        indices.reserve(512);
        expand_side(get_index_from_bytes(IP9[i].index_pointer_left), IPs,
                    indices);
        expand_side(get_index_from_bytes(IP9[i].index_pointer_right), IPs,
                    indices);
        expanded.push_back(std::move(indices));
    }
    IFV {
        if (!expanded.empty())
            std::cout << "Expanded chain[0] size = " << expanded[0].size()
                      << "\n";
    }
}

static inline void pr_expand_ip9_batched(
    int seed, const Layer_IP& IP9, Layer0_IDX& L0, Layer1_IDX& L1,
    Layer2_IDX& L2, Layer3_IDX& L3, Layer4_IDX& L4, Layer5_IDX& L5,
    Layer6_IDX& L6, Layer7_IDX& L7, Layer8_IDX& L8, Layer_IP& scratch_ip,
    std::vector<std::vector<size_t>>& expanded) {
    IFV {
        std::cout << "---------------------------------------------------------"
                     "----------------------\n"
                  << "[PR-Expand] Recomputing IP8..IP1 once per level (batched "
                     "mapping)...\n";
    }

    expanded.clear();
    expanded.reserve(IP9.size());
    std::vector<uint32_t> flat;
    flat.reserve(IP9.size() * 2);
    std::vector<std::pair<size_t, size_t>> ranges;
    ranges.reserve(IP9.size());
    for (size_t i = 0; i < IP9.size(); ++i) {
        size_t s = flat.size();
        flat.push_back(get_index_from_bytes(IP9[i].index_pointer_left));
        flat.push_back(get_index_from_bytes(IP9[i].index_pointer_right));
        ranges.emplace_back(s, s + 2);
    }

    auto rebuild_to_level = [&](int t) {
        fill_layer0_from_seed(L0, seed);
        set_index_batch(L0);
        if (t >= 1) {
            clear_vec(L1);
            clear_vec(scratch_ip);
            merge0_ip_inplace(L0, L1, scratch_ip);
            set_index_batch(L1);
        }
        if (t >= 2) {
            clear_vec(L2);
            clear_vec(scratch_ip);
            merge1_ip_inplace(L1, L2, scratch_ip);
            set_index_batch(L2);
        }
        if (t >= 3) {
            clear_vec(L3);
            clear_vec(scratch_ip);
            merge2_ip_inplace(L2, L3, scratch_ip);
            set_index_batch(L3);
        }
        if (t >= 4) {
            clear_vec(L4);
            clear_vec(scratch_ip);
            merge3_ip_inplace(L3, L4, scratch_ip);
            set_index_batch(L4);
        }
        if (t >= 5) {
            clear_vec(L5);
            clear_vec(scratch_ip);
            merge4_ip_inplace(L4, L5, scratch_ip);
            set_index_batch(L5);
        }
        if (t >= 6) {
            clear_vec(L6);
            clear_vec(scratch_ip);
            merge5_ip_inplace(L5, L6, scratch_ip);
            set_index_batch(L6);
        }
        if (t >= 7) {
            clear_vec(L7);
            clear_vec(scratch_ip);
            merge6_ip_inplace(L6, L7, scratch_ip);
            set_index_batch(L7);
        }
        if (t >= 8) {
            clear_vec(L8);
            clear_vec(scratch_ip);
            merge7_ip_inplace(L7, L8, scratch_ip);
            set_index_batch(L8);
        }
    };
    auto map_by_layer = [&](const Layer_IP& IPt) {
        std::vector<uint32_t> next;
        next.reserve(flat.size() * 2);
        for (uint32_t idx : flat) {
            if (idx >= IPt.size()) continue;
            const auto& ip = IPt[idx];
            next.push_back(get_index_from_bytes(ip.index_pointer_left));
            next.push_back(get_index_from_bytes(ip.index_pointer_right));
        }
        flat.swap(next);
        for (auto& rg : ranges) {
            size_t len = rg.second - rg.first;
            rg.first *= 2;
            rg.second = rg.first + len * 2;
        }
    };

    for (int t = 8; t >= 1; --t) {
        rebuild_to_level(t);
        map_by_layer(scratch_ip);
    }

    expanded.resize(IP9.size());
    for (size_t i = 0; i < IP9.size(); ++i) {
        const auto [s, e] = ranges[i];
        expanded[i].assign(flat.begin() + s, flat.begin() + e);
    }
    IFV {
        if (!expanded.empty())
            std::cout << "Expanded chain[0] size = " << expanded[0].size()
                      << "\n";
    }
}

class IPDiskWriter {
   public:
    IPDiskWriter() : f_(nullptr), cursor_(0) {}
    ~IPDiskWriter() { close(); }
    bool open(const char* p) {
        close();
        f_ = std::fopen(p, "wb");
        cursor_ = 0;
        return f_ != nullptr;
    }
    void close() {
        if (f_) {
            std::fclose(f_);
            f_ = nullptr;
        }
        cursor_ = 0;
    }
    uint64_t append_layer(const Item_IP* data, size_t n) {
        if (!f_ || !n) return cursor_;
        size_t bytes = n * sizeof(Item_IP);
        size_t w = std::fwrite(data, 1, bytes, f_);
        if (w != bytes) std::perror("fwrite");
        uint64_t off = cursor_;
        cursor_ += w;
        return off;
    }

   private:
    std::FILE* f_;
    uint64_t cursor_;
};
class IPDiskReader {
   public:
    IPDiskReader() : f_(nullptr) {}
    ~IPDiskReader() { close(); }
    bool open(const char* p) {
        close();
        f_ = std::fopen(p, "rb");
        return f_ != nullptr;
    }
    void close() {
        if (f_) {
            std::fclose(f_);
            f_ = nullptr;
        }
    }
    bool read_slice(uint64_t off, uint32_t cnt, Layer_IP& out) {
        if (!f_) return false;
        if (std::fseek(f_, (long)off, SEEK_SET) != 0) {
            std::perror("fseek");
            return false;
        }
        out.resize(cnt);
        size_t bytes = size_t(cnt) * sizeof(Item_IP);
        size_t r = std::fread(out.data(), 1, bytes, f_);
        if (r != bytes) {
            std::perror("fread");
            return false;
        }
        return true;
    }

   private:
    std::FILE* f_;
};

static inline void run_cip_em_forward_and_dump(
    int seed, const std::string& em_path, IPDiskManifest& man, Layer0_IDX& L0,
    Layer1_IDX& L1, Layer2_IDX& L2, Layer3_IDX& L3, Layer4_IDX& L4,
    Layer5_IDX& L5, Layer6_IDX& L6, Layer7_IDX& L7, Layer8_IDX& L8,
    Layer9_IDX& L9, Layer_IP& /*IP1_unused*/, Layer_IP& IP9,
    Layer_IP& scratch_ip) {
    IPDiskWriter W;
    if (!W.open(em_path.c_str())) {
        std::cerr << "Cannot open EM file\n";
        return;
    }

    fill_layer0_from_seed(L0, seed);
    set_index_batch(L0);

    clear_vec(L1);
    clear_vec(scratch_ip);
    merge0_ip_inplace(L0, L1, scratch_ip);
    set_index_batch(L1);
    man.ip[1].offset = W.append_layer(scratch_ip.data(), scratch_ip.size());
    man.ip[1].count = (uint32_t)scratch_ip.size();
    clear_vec(scratch_ip);

    clear_vec(L2);
    clear_vec(scratch_ip);
    merge1_ip_inplace(L1, L2, scratch_ip);
    set_index_batch(L2);
    man.ip[2].offset = W.append_layer(scratch_ip.data(), scratch_ip.size());
    man.ip[2].count = (uint32_t)scratch_ip.size();
    clear_vec(scratch_ip);

    clear_vec(L3);
    clear_vec(scratch_ip);
    merge2_ip_inplace(L2, L3, scratch_ip);
    set_index_batch(L3);
    man.ip[3].offset = W.append_layer(scratch_ip.data(), scratch_ip.size());
    man.ip[3].count = (uint32_t)scratch_ip.size();
    clear_vec(scratch_ip);

    clear_vec(L4);
    clear_vec(scratch_ip);
    merge3_ip_inplace(L3, L4, scratch_ip);
    set_index_batch(L4);
    man.ip[4].offset = W.append_layer(scratch_ip.data(), scratch_ip.size());
    man.ip[4].count = (uint32_t)scratch_ip.size();
    clear_vec(scratch_ip);

    clear_vec(L5);
    clear_vec(scratch_ip);
    merge4_ip_inplace(L4, L5, scratch_ip);
    set_index_batch(L5);
    man.ip[5].offset = W.append_layer(scratch_ip.data(), scratch_ip.size());
    man.ip[5].count = (uint32_t)scratch_ip.size();
    clear_vec(scratch_ip);

    clear_vec(L6);
    clear_vec(scratch_ip);
    merge5_ip_inplace(L5, L6, scratch_ip);
    set_index_batch(L6);
    man.ip[6].offset = W.append_layer(scratch_ip.data(), scratch_ip.size());
    man.ip[6].count = (uint32_t)scratch_ip.size();
    clear_vec(scratch_ip);

    clear_vec(L7);
    clear_vec(scratch_ip);
    merge6_ip_inplace(L6, L7, scratch_ip);
    set_index_batch(L7);
    man.ip[7].offset = W.append_layer(scratch_ip.data(), scratch_ip.size());
    man.ip[7].count = (uint32_t)scratch_ip.size();
    clear_vec(scratch_ip);

    clear_vec(L8);
    clear_vec(scratch_ip);
    merge7_ip_inplace(L7, L8, scratch_ip);
    set_index_batch(L8);
    man.ip[8].offset = W.append_layer(scratch_ip.data(), scratch_ip.size());
    man.ip[8].count = (uint32_t)scratch_ip.size();
    clear_vec(scratch_ip);

    clear_vec(L9);
    clear_vec(IP9);
    merge8_ip_inplace(L8, L9, IP9);
    set_index_batch(L9);
    W.close();

    IFV {
        std::cout << "[CIP-EM] Dumped IP1..IP8 to '" << em_path
                  << "'.  IP9=" << IP9.size() << "\n";
    }
}

static inline void em_expand_ip9_from_disk(
    const IPDiskManifest& man, const std::string& em_path,
    const Layer_IP& /*IP1_resident_ignored*/, const Layer_IP& IP9,
    Layer_IP& scratch_ip, std::vector<std::vector<size_t>>& expanded) {
    expanded.clear();
    expanded.reserve(IP9.size());
    IPDiskReader R;
    if (!R.open(em_path.c_str())) {
        std::cerr << "Cannot open EM file\n";
        return;
    }

    std::vector<uint32_t> flat;
    flat.reserve(IP9.size() * 2);
    std::vector<std::pair<size_t, size_t>> ranges;
    ranges.reserve(IP9.size());
    for (size_t i = 0; i < IP9.size(); ++i) {
        size_t s = flat.size();
        flat.push_back(get_index_from_bytes(IP9[i].index_pointer_left));
        flat.push_back(get_index_from_bytes(IP9[i].index_pointer_right));
        ranges.emplace_back(s, s + 2);
    }

    auto map_by_layer = [&](const Layer_IP& IPt) {
        std::vector<uint32_t> next;
        next.reserve(flat.size() * 2);
        for (uint32_t idx : flat) {
            if (idx >= IPt.size()) continue;
            const auto& ip = IPt[idx];
            next.push_back(get_index_from_bytes(ip.index_pointer_left));
            next.push_back(get_index_from_bytes(ip.index_pointer_right));
        }
        flat.swap(next);
        for (auto& rg : ranges) {
            size_t len = rg.second - rg.first;
            rg.first *= 2;
            rg.second = rg.first + len * 2;
        }
    };

    // read IP8..IP1 sequentially into scratch_ip
    for (int t = 8; t >= 1; --t) {
        if (man.ip[t].count == 0) {
            std::cerr << "[EM] manifest missing IP" << t << "\n";
            R.close();
            return;
        }
        if (!R.read_slice(man.ip[t].offset, man.ip[t].count, scratch_ip)) {
            std::cerr << "[EM] read IP" << t << " failed\n";
            R.close();
            return;
        }
        map_by_layer(scratch_ip);
        clear_vec(scratch_ip);
    }
    R.close();

    expanded.resize(IP9.size());
    for (size_t i = 0; i < IP9.size(); ++i) {
        const auto [s, e] = ranges[i];
        expanded[i].assign(flat.begin() + s, flat.begin() + e);
    }
    IFV {
        if (!expanded.empty())
            std::cout << "Expanded chain[0] size = " << expanded[0].size()
                      << "\n";
    }
}

// ---------- Check ----------
static inline bool is_trivial_solution(const std::vector<size_t>& chain) {
    std::unordered_map<size_t, size_t> cnt;
    cnt.reserve(chain.size() * 2);
    for (size_t x : chain) cnt[x]++;
    for (auto& kv : cnt)
        if ((kv.second & 1) != 0) return false;
    return true;
}

static inline size_t check_zero_xor_dual(
    const Layer0_IDX& L0, const std::vector<std::vector<size_t>>& expanded) {
    auto is_zero_200 = [](const std::array<uint8_t, 25>& acc) {
        for (uint8_t b : acc) {
            if (b) return false;
        }
        return true;
    };
    auto is_zero_180 = [](const std::array<uint8_t, 25>& acc) {
        for (int i = 0; i <= 21; ++i)
            if (acc[(size_t)i] != 0) return false;
        if ((acc[22] & 0x0Fu) != 0) return false;
        return true;
    };

    const size_t total = expanded.size();
    size_t trivial = 0, valid200 = 0, valid180 = 0;

    for (size_t ci = 0; ci < total; ++ci) {
        const auto& chain = expanded[ci];
        if (is_trivial_solution(chain)) {
            ++trivial;
            continue;
        }

        std::array<uint8_t, 25> acc{};
        acc.fill(0);
        bool is_180 = false;
        bool is_200 = false;

        for (size_t idx : chain) {
            if (idx >= L0.size()) {
                IFV {
                    std::printf(
                        "  Chain %zu: index %zu out of range (L0 size=%zu)\n",
                        ci, idx, L0.size());
                }
                goto next;
            }
            const auto& item = L0[idx];
            for (size_t j = 0; j < 25; ++j) acc[j] ^= item.XOR[j];
        }

        is_180 = is_zero_180(acc);
        is_200 = is_zero_200(acc);

        if (is_180) ++valid180;
        if (is_200) ++valid200;

        // Print detailed XOR result for each non-trivial solution
        IFV {
            std::printf("  Chain %zu (size=%zu): XOR result = ", ci,
                        chain.size());
            for (size_t j = 0; j < 25; ++j) std::printf("%02x", acc[j]);
            if (is_200) {
                std::printf(" ✓ VALID 200-bit (all zeros)\n");
            } else if (is_180) {
                std::printf(" ✓ VALID 180-bit (first 180 bits zero)\n");
            } else {
                std::printf(" ✗ INVALID (non-zero)\n");
            }

            // Print the indices for verification
            if (is_200 || !is_180) {
                std::printf("    Indices: ");
                size_t show_n = std::min<size_t>(16, chain.size());
                for (size_t i = 0; i < show_n; ++i)
                    std::printf("%zx ", chain[i]);
                if (chain.size() > show_n)
                    std::printf("... (total %zu)", chain.size());
                std::printf("\n");
            }
        }
    next:;
    }

    IFV {
        std::cout << "---------------------------------------------------------"
                     "----------------------\n";
        std::cout << "Solution Statistics:\n";
        std::cout << "  Total chains: " << total << "\n";
        std::cout << "  Trivial chains: " << trivial << "\n";
        std::cout << "  Valid on used 180 bits: " << valid180
                  << "  (should equal " << (total - trivial) << ")\n";
        std::cout << "  Valid 200-bit solutions: " << valid200 << "\n";
        std::cout << "========================================================="
                     "======================\n";
    }
    return valid200;
}

// ---------- Public entry helpers (used by main) ----------
void run_cip_and_expand_seed(int seed, std::vector<std::vector<size_t>>& chains,
                             Layer0_IDX& L0, Layer1_IDX& L1, Layer2_IDX& L2,
                             Layer3_IDX& L3, Layer4_IDX& L4, Layer5_IDX& L5,
                             Layer6_IDX& L6, Layer7_IDX& L7, Layer8_IDX& L8,
                             Layer9_IDX& L9, Layer_IP& IP1, Layer_IP& IP2,
                             Layer_IP& IP3, Layer_IP& IP4, Layer_IP& IP5,
                             Layer_IP& IP6, Layer_IP& IP7, Layer_IP& IP8,
                             Layer_IP& IP9) {
    run_cip_forward_seed(seed, L0, L1, L2, L3, L4, L5, L6, L7, L8, L9, IP1, IP2,
                         IP3, IP4, IP5, IP6, IP7, IP8, IP9);
    cip_expand_ip9_resident(IP1, IP2, IP3, IP4, IP5, IP6, IP7, IP8, IP9,
                            chains);
}

void run_pr_and_expand_seed(int seed, std::vector<std::vector<size_t>>& chains,
                            Layer0_IDX& L0, Layer1_IDX& L1, Layer2_IDX& L2,
                            Layer3_IDX& L3, Layer4_IDX& L4, Layer5_IDX& L5,
                            Layer6_IDX& L6, Layer7_IDX& L7, Layer8_IDX& L8,
                            Layer9_IDX& L9, Layer_IP& scratch_ip,
                            const Layer_IP& IP9) {
    IFV {
        std::cout << "---------------------------------------------------------"
                     "----------------------\n"
                  << "[CIP-PR] Building forward chain (PR: keep none of "
                     "IP1..IP8)...\n";
    }

    fill_layer0_from_seed(L0, seed);
    set_index_batch(L0);
    IFV { std::cout << "Layer 0 size: " << L0.size() << std::endl; }
    clear_vec(L1);
    clear_vec(scratch_ip);
    merge0_ip_inplace(L0, L1, scratch_ip);
    set_index_batch(L1);
    IFV { std::cout << "Layer 1 size: " << L1.size() << std::endl; }
    clear_vec(L2);
    clear_vec(scratch_ip);
    merge1_ip_inplace(L1, L2, scratch_ip);
    set_index_batch(L2);
    IFV { std::cout << "Layer 2 size: " << L2.size() << std::endl; }
    clear_vec(L3);
    clear_vec(scratch_ip);
    merge2_ip_inplace(L2, L3, scratch_ip);
    set_index_batch(L3);
    IFV { std::cout << "Layer 3 size: " << L3.size() << std::endl; }
    clear_vec(L4);
    clear_vec(scratch_ip);
    merge3_ip_inplace(L3, L4, scratch_ip);
    set_index_batch(L4);
    IFV { std::cout << "Layer 4 size: " << L4.size() << std::endl; }
    clear_vec(L5);
    clear_vec(scratch_ip);
    merge4_ip_inplace(L4, L5, scratch_ip);
    set_index_batch(L5);
    IFV { std::cout << "Layer 5 size: " << L5.size() << std::endl; }
    clear_vec(L6);
    clear_vec(scratch_ip);
    merge5_ip_inplace(L5, L6, scratch_ip);
    set_index_batch(L6);
    IFV { std::cout << "Layer 6 size: " << L6.size() << std::endl; }
    clear_vec(L7);
    clear_vec(scratch_ip);
    merge6_ip_inplace(L6, L7, scratch_ip);
    set_index_batch(L7);
    IFV { std::cout << "Layer 7 size: " << L7.size() << std::endl; }
    clear_vec(L8);
    clear_vec(scratch_ip);
    merge7_ip_inplace(L7, L8, scratch_ip);
    set_index_batch(L8);
    IFV { std::cout << "Layer 8 size: " << L8.size() << std::endl; }
    clear_vec(L9);
    clear_vec(const_cast<Layer_IP&>(IP9));
    merge8_ip_inplace(L8, L9, const_cast<Layer_IP&>(IP9));
    set_index_batch(L9);
    IFV {
        std::cout << "Layer 9 size: " << L9.size() << std::endl
                  << "[CIP-PR] Finished.  IP9=" << IP9.size()
                  << "  (IP1..IP8 not resident)\n";
    }

    pr_expand_ip9_batched(seed, IP9, L0, L1, L2, L3, L4, L5, L6, L7, L8,
                          scratch_ip, chains);
}

void run_em_and_expand_seed(int seed, const std::string& em_path,
                            std::vector<std::vector<size_t>>& chains,
                            IPDiskManifest& man, Layer0_IDX& L0, Layer1_IDX& L1,
                            Layer2_IDX& L2, Layer3_IDX& L3, Layer4_IDX& L4,
                            Layer5_IDX& L5, Layer6_IDX& L6, Layer7_IDX& L7,
                            Layer8_IDX& L8, Layer9_IDX& L9, Layer_IP& IP1,
                            Layer_IP& IP9, Layer_IP& scratch_ip) {
    run_cip_em_forward_and_dump(seed, em_path, man, L0, L1, L2, L3, L4, L5, L6,
                                L7, L8, L9, IP1, IP9, scratch_ip);
    em_expand_ip9_from_disk(man, em_path, IP1, IP9, scratch_ip, chains);
}

size_t check_with_refilled_L0_from_seed(
    int seed, const std::vector<std::vector<size_t>>& chains, Layer0_IDX& L0) {
    fill_layer0_from_seed(L0, seed);
    return check_zero_xor_dual(L0, chains);
}
