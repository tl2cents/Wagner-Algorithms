#pragma once
#include <cstring>
#include <stdexcept>

extern "C" {
#include "blake2.h"
}

struct ZcashEquihashHasher {
    static constexpr uint32_t N_BITS = 200;
    static constexpr uint32_t K_ROUNDS = 9;
    static constexpr size_t OUT_LEN =
        50;  // Zcash Equihash outputs 50 bytes per index

    blake2b_state mid_;  // midstate after absorbing header||nonce

    static inline void make_personal(uint8_t pers[16], uint32_t n = N_BITS,
                                     uint32_t k = K_ROUNDS) {
        static const uint8_t ZPow[8] = {'Z', 'c', 'a', 's', 'h', 'P', 'o', 'W'};
        std::memcpy(pers, ZPow, 8);
        pers[8] = uint8_t(n & 0xFF);
        pers[9] = uint8_t((n >> 8) & 0xFF);
        pers[10] = uint8_t((n >> 16) & 0xFF);
        pers[11] = uint8_t((n >> 24) & 0xFF);
        pers[12] = uint8_t(k & 0xFF);
        pers[13] = uint8_t((k >> 8) & 0xFF);
        pers[14] = uint8_t((k >> 16) & 0xFF);
        pers[15] = uint8_t((k >> 24) & 0xFF);
    }

    // Initialize midstate: digest_len=50,
    // personal="ZcashPoW"||LE32(n)||LE32(k). Absorb header (arbitrary length)
    // then 32-byte nonce.
    void init_midstate(const uint8_t* header, size_t header_len,
                       const uint8_t nonce[32], uint32_t n = N_BITS,
                       uint32_t k = K_ROUNDS) {
        uint8_t pers[16];
        make_personal(pers, n, k);
        blake2b_param P;
        std::memset(&P, 0, sizeof(P));
        P.digest_length = uint8_t(OUT_LEN);
        P.fanout = 1;
        P.depth = 1;
        std::memcpy(P.personal, pers, 16);
        if (blake2b_init_param(&mid_, &P) != 0)
            throw std::runtime_error("blake2b_init_param failed");
        if (header && header_len) blake2b_update(&mid_, header, header_len);
        blake2b_update(&mid_, nonce, 32);
    }

    // Hash a single 32-bit index with midstate reuse.
    inline void hash_index(uint32_t idx, uint8_t* out50) const {
        blake2b_state S = mid_;
        uint8_t le[4] = {uint8_t(idx & 0xFF), uint8_t((idx >> 8) & 0xFF),
                         uint8_t((idx >> 16) & 0xFF),
                         uint8_t((idx >> 24) & 0xFF)};
        blake2b_update(&S, le, 4);
        blake2b_final(&S, out50, OUT_LEN);
    }
};