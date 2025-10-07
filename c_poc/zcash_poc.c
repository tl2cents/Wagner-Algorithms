/*------------------------------------------------------------------------------
* zcash_poc.c
 *
 * Proof-of-Concept (PoC) implementation for the experimental study of
 * Wagner’s algorithm and its optimized memory–time tradeoffs.
 *
 * This code illustrates the core ideas and implementation principles
 * behind our exploration of Wagner’s algorithm and its optimized
 * memory–time tradeoffs. It focuses on algorithmic clarity and correctness,
 * rather than on peak performance, serving as a reproducible
 * proof-of-concept for experimental and structural analysis.
 *
 * Build:
     gcc -std=c11 -Ofast -flto -march=native -funroll-loops -DNDEBUG \
         zcash_poc.c third_party/BLAKE2/sse/blake2b*.c \
         -Ithird_party/BLAKE2/sse -o zcash_poc
 *
 * CLI:
 *   ./zcash_poc [plain_ip | ip_pr | ip_em] [external_path] [--repeat=R]
 *   ./zcash_poc --all [external_path_for_ip_em] [--repeat=R]
 *
 * Benchmark Results:
    Algorithm   ok/rep |  time_avg     std     min     max | peak_rss(MiB)     std     min     max
    ---------------------------------------------------------------------------------------
    plain_ip    30/30  |      2.33    0.09    2.20    2.57 |         172.3     2.9   166.7   179.7
    ip_pr       30/30  |     12.88    0.27   12.56   13.81 |          89.4     1.6    85.9    92.6
    ip_em       30/30  |      2.37    0.08    2.20    2.57 |          77.5     0.5    77.0    79.2
 *------------------------------------------------------------------------------*/

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#include "blake2.h"

/*------------------------------------------------------------------------------
 * Instance params
 *------------------------------------------------------------------------------*/
enum { N_BITS = 200, LGK = 9, ELL = 20 };
enum { N = (1u << (ELL + 1)) };         /* 2^21 */
enum { OUT_BYTES0 = (N_BITS + 7) / 8 }; /* 25 bytes */

/*------------------------------------------------------------------------------
 * Helpers: u24, timing, madvise wrappers
 *------------------------------------------------------------------------------*/
typedef struct __attribute__((packed)) {
  uint8_t a[3];
  uint8_t b[3];
} ipair24_t;

static inline void store_u24_le(uint8_t dst[3], uint32_t x) {
  dst[0] = (uint8_t)x;
  dst[1] = (uint8_t)(x >> 8);
  dst[2] = (uint8_t)(x >> 16);
}
static inline uint32_t load_u24_le(const uint8_t src[3]) {
  return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16);
}

static inline void advise_seq_willneed(void* addr, size_t len) {
#if defined(MADV_SEQUENTIAL) && defined(MADV_WILLNEED)
  (void)!madvise(addr, len, MADV_SEQUENTIAL);
  (void)!madvise(addr, len, MADV_WILLNEED);
#elif defined(POSIX_MADV_SEQUENTIAL) && defined(POSIX_MADV_WILLNEED)
  (void)!posix_madvise(addr, len, POSIX_MADV_SEQUENTIAL);
  (void)!posix_madvise(addr, len, POSIX_MADV_WILLNEED);
#else
  (void)addr;
  (void)len;
#endif
}
static inline void advise_dontneed(void* addr, size_t len) {
#if defined(MADV_DONTNEED)
  (void)!madvise(addr, len, MADV_DONTNEED);
#elif defined(POSIX_MADV_DONTNEED)
  (void)!posix_madvise(addr, len, POSIX_MADV_DONTNEED);
#else
  (void)addr;
  (void)len;
#endif
}

static double wall_now_sec(void) {
  struct timespec ts;
#if defined(CLOCK_MONOTONIC_RAW)
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
  clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
static double rss_mib_from_rusage(const struct rusage* ru) {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
  return (double)ru->ru_maxrss / (1024.0 * 1024.0);
#else
  return (double)ru->ru_maxrss / 1024.0;
#endif
}

/*------------------------------------------------------------------------------
 * Stage list
 *------------------------------------------------------------------------------*/
typedef struct {
  uint8_t* buf;       /* values buffer, LE byte-slices */
  uint32_t len;       /* #items */
  uint8_t item_bytes; /* 25 -> 23 -> ... */
} stage_list_t;

/*------------------------------------------------------------------------------
 * Workspace for 20-bit bucketing (shared)
 *------------------------------------------------------------------------------*/
static uint32_t *CNT = NULL, *OFFS = NULL, *ORDER = NULL;
static size_t ORDER_CAP = 0;

static void ensure_workspace(uint32_t cur_len) {
  const uint32_t B = 1u << ELL;
  if (!CNT) CNT = (uint32_t*)calloc(B, sizeof(uint32_t));
  if (!OFFS) OFFS = (uint32_t*)malloc(B * sizeof(uint32_t));
  if (!CNT || !OFFS) {
    fprintf(stderr, "alloc CNT/OFFS failed\n");
    exit(1);
  }
  if (ORDER_CAP < cur_len) {
    ORDER = (uint32_t*)realloc(ORDER, (size_t)cur_len * sizeof(uint32_t));
    if (!ORDER) {
      fprintf(stderr, "alloc ORDER failed\n");
      exit(1);
    }
    ORDER_CAP = cur_len;
  }
}
static void free_workspace(void) {
  free(CNT);
  free(OFFS);
  free(ORDER);
  CNT = NULL;
  OFFS = NULL;
  ORDER = NULL;
  ORDER_CAP = 0;
}

/*------------------------------------------------------------------------------
 * Bit helpers
 *------------------------------------------------------------------------------*/
static inline void to_le_25(uint8_t out_le[25], const uint8_t digest[25]) {
  for (int i = 0; i < 25; ++i) out_le[i] = digest[24 - i];
}
static inline uint32_t low20_from_le(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         (((uint32_t)p[2] & 0x0Fu) << 16);
}

/* fixed xor >> 20 for byte-slices (no branches) */
static inline void xor_shr20_store(uint8_t* out, const uint8_t* a,
                                   const uint8_t* b, size_t in_bytes,
                                   size_t out_bytes) {
  /* (a^b) >> 20 = >> (2 bytes + 4 bits) */
  const size_t byte_shift = 2;
  const unsigned bit_shift = 4; /* high 4 from next byte */

  /* tmp = a^b */
  /* Directly read from a/b; we don't materialize full tmp. */
  for (size_t j = 0; j < out_bytes; ++j) {
    size_t i0 = j + byte_shift;
    uint32_t lo = 0, hi = 0;
    if (i0 < in_bytes) {
      lo = (uint32_t)(a[i0] ^ b[i0]);
    }
    if (i0 + 1 < in_bytes) {
      hi = (uint32_t)(a[i0 + 1] ^ b[i0 + 1]);
    }
    out[j] = (uint8_t)((lo >> bit_shift) | (hi << (8 - bit_shift)));
  }
}

/*------------------------------------------------------------------------------
 * Hashing: BLAKE2b(200). Midstate clone + LE u32 counter.
 *------------------------------------------------------------------------------*/
static void compute_hash_list(stage_list_t* L0, const uint8_t nonce[16]) {
  L0->len = (uint32_t)N;
  L0->item_bytes = OUT_BYTES0; /* 25 */
  L0->buf = (uint8_t*)malloc((size_t)L0->len * L0->item_bytes);
  if (!L0->buf) {
    fprintf(stderr, "alloc L0 failed\n");
    exit(1);
  }

  blake2b_state base;
  blake2b_init(&base, OUT_BYTES0);
  blake2b_update(&base, nonce, 16);

  for (uint32_t i = 0; i < L0->len; ++i) {
    blake2b_state S = base; /* clone */
    uint32_t ctr = i;       /* LE 4B counter */
    blake2b_update(&S, (uint8_t*)&ctr, sizeof(ctr));
    uint8_t digest[OUT_BYTES0];
    blake2b_final(&S, digest, OUT_BYTES0);
    to_le_25(&L0->buf[(size_t)i * L0->item_bytes], digest);
  }
}

/*------------------------------------------------------------------------------
 * Reorder into 20-bit buckets (stable).
 *------------------------------------------------------------------------------*/
static void reorder_into_bucket_order(stage_list_t* cur,
                                      uint32_t* idmap_or_null) {
  const uint8_t in_bytes = cur->item_bytes;
  const uint32_t B = 1u << ELL;
  ensure_workspace(cur->len);

  memset(CNT, 0, (size_t)B * sizeof(uint32_t));
  for (uint32_t i = 0; i < cur->len; ++i) {
    uint32_t key = low20_from_le(&cur->buf[(size_t)i * in_bytes]);
    CNT[key]++;
  }
  uint64_t total = 0;
  for (uint32_t k = 0; k < B; ++k) {
    OFFS[k] = (uint32_t)total;
    total += CNT[k];
  }
  for (uint32_t i = 0; i < cur->len; ++i) {
    uint32_t key = low20_from_le(&cur->buf[(size_t)i * in_bytes]);
    ORDER[OFFS[key]++] = i;
  }
  for (uint32_t k = B; k-- > 0;) OFFS[k] = (k == 0 ? 0 : OFFS[k - 1]);

  /* in-place permutation by cycles */
  uint8_t* tmp = (uint8_t*)malloc(in_bytes);
  uint8_t* vis = (uint8_t*)calloc(cur->len, 1);
  if (!tmp || !vis) {
    fprintf(stderr, "reorder temp alloc\n");
    exit(1);
  }

  uint32_t idtmp = 0;
  for (uint32_t s = 0; s < cur->len; ++s) {
    if (vis[s]) continue;
    uint32_t src = ORDER[s];
    if (src == s) {
      vis[s] = 1;
      continue;
    }

    memcpy(tmp, &cur->buf[(size_t)s * in_bytes], in_bytes);
    if (idmap_or_null) idtmp = idmap_or_null[s];
    uint32_t curpos = s;
    while (src != s) {
      memcpy(&cur->buf[(size_t)curpos * in_bytes],
             &cur->buf[(size_t)src * in_bytes], in_bytes);
      if (idmap_or_null) idmap_or_null[curpos] = idmap_or_null[src];
      vis[curpos] = 1;
      curpos = src;
      src = ORDER[curpos];
    }
    memcpy(&cur->buf[(size_t)curpos * in_bytes], tmp, in_bytes);
    if (idmap_or_null) idmap_or_null[curpos] = idtmp;
    vis[curpos] = 1;
  }
  free(tmp);
  free(vis);
}

/*------------------------------------------------------------------------------
 * Spill buffer
 *------------------------------------------------------------------------------*/
typedef struct {
  uint8_t* buf;
  size_t size, cap;
} spill_t;
static void spill_init(spill_t* s, size_t reserve) {
  s->size = 0;
  s->cap = reserve ? reserve : (1u << 20);
  s->buf = (uint8_t*)malloc(s->cap);
  if (!s->buf) {
    fprintf(stderr, "spill alloc\n");
    exit(1);
  }
}
static void spill_reserve(spill_t* s, size_t need) {
  if (need <= s->cap) return;
  size_t nc = s->cap;
  while (nc < need) {
    if (nc > SIZE_MAX / 3) {
      fprintf(stderr, "spill overflow\n");
      exit(1);
    }
    nc = nc * 3 / 2 + (1 << 20);
  }
  uint8_t* p = (uint8_t*)realloc(s->buf, nc);
  if (!p) {
    fprintf(stderr, "spill realloc\n");
    exit(1);
  }
  s->buf = p;
  s->cap = nc;
}
static inline void spill_append(spill_t* s, const void* src, size_t n) {
  size_t need = s->size + n;
  spill_reserve(s, need);
  memcpy(s->buf + s->size, src, n);
  s->size = need;
}
static void spill_free(spill_t* s) {
  free(s->buf);
  s->buf = NULL;
  s->size = s->cap = 0;
}

/*------------------------------------------------------------------------------
 * Estimate Σ C(m_k,2) from CNT
 *------------------------------------------------------------------------------*/
static inline uint64_t estimate_pairs_upper_bound_from_cnt(void) {
  const uint32_t B = 1u << ELL;
  uint64_t est = 0;
  for (uint32_t k = 0; k < B; ++k) {
    uint64_t m = CNT[k];
    if (m >= 2) est += (m * (m - 1)) >> 1;
  }
  return est;
}

/*------------------------------------------------------------------------------
 * Pairs sink/source abstraction
 *------------------------------------------------------------------------------*/
typedef struct pairs_sink_s pairs_sink_t;
typedef struct pairs_source_s pairs_source_t;

typedef struct {
  void* (*begin_round)(pairs_sink_t*, int round_idx, uint64_t expected_pairs);
  void (*emit_front_pair)(pairs_sink_t*, void* ctx, uint32_t ia, uint32_t ib);
  void (*emit_spill_pair)(pairs_sink_t*, void* ctx, uint32_t ia, uint32_t ib);
  uint64_t (*end_round)(pairs_sink_t*, void* ctx);
  pairs_source_t* (*as_source)(pairs_sink_t*, int round_idx);
  void (*destroy)(pairs_sink_t*);
} pairs_sink_vtbl_t;

struct pairs_sink_s {
  const pairs_sink_vtbl_t* v;
};

typedef struct {
  uint32_t (*length)(pairs_source_t*);
  void (*read_pair)(pairs_source_t*, uint32_t idx, uint32_t* a, uint32_t* b);
  void (*close)(pairs_source_t*);
} pairs_source_vtbl_t;

struct pairs_source_s {
  const pairs_source_vtbl_t* v;
};

/*------------------------------------------------------------------------------
 * MemSink
 *------------------------------------------------------------------------------*/
typedef struct {
  pairs_sink_t base;
  ipair24_t* pairs[LGK];
  uint32_t lens[LGK];
  size_t mapped_bytes[LGK];
  uint8_t is_mmap[LGK];

  ipair24_t** spill_vec;
  size_t *spill_sz, *spill_cap;
} mem_sink_t;

static void* mem_begin_round(pairs_sink_t* s, int round_idx,
                             uint64_t expected_pairs) {
  mem_sink_t* M = (mem_sink_t*)s;
  size_t bytes = (size_t)expected_pairs * sizeof(ipair24_t);
  ipair24_t* p = NULL;
  size_t mapped = 0;
  uint8_t is_mmap = 0;
  if (bytes) {
    void* addr = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr != MAP_FAILED) {
      p = (ipair24_t*)addr;
      mapped = bytes;
      is_mmap = 1;
      advise_seq_willneed(p, bytes);
    } else {
      p = (ipair24_t*)malloc(bytes);
      if (!p) {
        perror("mem malloc");
        exit(1);
      }
      mapped = bytes;
      is_mmap = 0;
    }
  }
  M->pairs[round_idx] = p;
  M->lens[round_idx] = 0;
  M->mapped_bytes[round_idx] = mapped;
  M->is_mmap[round_idx] = is_mmap;
  if (!M->spill_vec) {
    M->spill_vec = (ipair24_t**)calloc(LGK, sizeof(ipair24_t*));
    M->spill_sz = (size_t*)calloc(LGK, sizeof(size_t));
    M->spill_cap = (size_t*)calloc(LGK, sizeof(size_t));
    if (!M->spill_vec || !M->spill_sz || !M->spill_cap) {
      fprintf(stderr, "mem spill alloc\n");
      exit(1);
    }
  }
  return (void*)(intptr_t)round_idx;
}
static void mem_emit_front_pair(pairs_sink_t* s, void* ctx, uint32_t ia,
                                uint32_t ib) {
  mem_sink_t* M = (mem_sink_t*)s;
  int r = (int)(intptr_t)ctx;
  uint32_t pos = M->lens[r];
  if (!M->pairs[r]) {
    fprintf(stderr, "mem_emit: null store\n");
    exit(1);
  }
  if ((size_t)pos * sizeof(ipair24_t) >= M->mapped_bytes[r]) {
    fprintf(stderr, "mem_emit: overflow\n");
    exit(1);
  }
  ipair24_t* p = &M->pairs[r][pos];
  store_u24_le(p->a, ia);
  store_u24_le(p->b, ib);
  M->lens[r] = pos + 1;
}
static void mem_emit_spill_pair(pairs_sink_t* s, void* ctx, uint32_t ia,
                                uint32_t ib) {
  mem_sink_t* M = (mem_sink_t*)s;
  int r = (int)(intptr_t)ctx;
  size_t sz = M->spill_sz[r], cap = M->spill_cap[r];
  if (sz == cap) {
    size_t nc = cap ? (cap * 3 / 2 + (1 << 10)) : (size_t)1 << 16;
    ipair24_t* np =
        (ipair24_t*)realloc(M->spill_vec[r], nc * sizeof(ipair24_t));
    if (!np) {
      fprintf(stderr, "mem spill realloc\n");
      exit(1);
    }
    M->spill_vec[r] = np;
    M->spill_cap[r] = nc;
  }
  ipair24_t* p = &M->spill_vec[r][M->spill_sz[r]++];
  store_u24_le(p->a, ia);
  store_u24_le(p->b, ib);
}
static uint64_t mem_end_round(pairs_sink_t* s, void* ctx) {
  mem_sink_t* M = (mem_sink_t*)s;
  int r = (int)(intptr_t)ctx;
  size_t front = M->lens[r], spill = M->spill_sz[r];
  if (spill) {
    if (((front + spill) * sizeof(ipair24_t)) > M->mapped_bytes[r]) {
      fprintf(stderr, "mem_end: overflow\n");
      exit(1);
    }
    memcpy(&M->pairs[r][front], M->spill_vec[r], spill * sizeof(ipair24_t));
  }
  free(M->spill_vec[r]);
  M->spill_vec[r] = NULL;
  M->spill_sz[r] = M->spill_cap[r] = 0;
  size_t used = (front + spill) * sizeof(ipair24_t);
  if (M->is_mmap[r] && M->mapped_bytes[r] > used) {
    size_t tail = M->mapped_bytes[r] - used;
    if (tail) advise_dontneed((uint8_t*)M->pairs[r] + used, tail);
  }
  M->lens[r] = (uint32_t)(front + spill);
  return M->lens[r];
}

/* mem source */
typedef struct {
  pairs_source_t base;
  const ipair24_t* pairs;
  uint32_t len;
} mem_source_t;
static uint32_t mem_src_length(pairs_source_t* src) {
  return ((mem_source_t*)src)->len;
}
static void mem_src_read(pairs_source_t* src, uint32_t idx, uint32_t* a,
                         uint32_t* b) {
  mem_source_t* S = (mem_source_t*)src;
  assert(idx < S->len);
  const ipair24_t* p = &S->pairs[idx];
  *a = load_u24_le(p->a);
  *b = load_u24_le(p->b);
}
static void mem_src_close(pairs_source_t* src) { (void)src; }

static const pairs_source_vtbl_t MEM_SRC_VTBL = {mem_src_length, mem_src_read,
                                                 mem_src_close};

static pairs_source_t* mem_as_source(pairs_sink_t* s, int round_idx) {
  mem_sink_t* M = (mem_sink_t*)s;
  mem_source_t* S = (mem_source_t*)malloc(sizeof(mem_source_t));
  if (!S) {
    fprintf(stderr, "mem_as_source malloc\n");
    exit(1);
  }
  S->base.v = &MEM_SRC_VTBL;
  S->pairs = M->pairs[round_idx];
  S->len = M->lens[round_idx];
  return (pairs_source_t*)S;
}
static void mem_destroy(pairs_sink_t* s) {
  mem_sink_t* M = (mem_sink_t*)s;
  for (int r = 0; r < LGK; ++r) {
    if (M->pairs[r]) {
      if (M->is_mmap[r])
        munmap(M->pairs[r], M->mapped_bytes[r]);
      else
        free(M->pairs[r]);
    }
  }
  if (M->spill_vec) {
    for (int r = 0; r < LGK; ++r) free(M->spill_vec[r]);
    free(M->spill_vec);
    free(M->spill_sz);
    free(M->spill_cap);
  }
  free(M);
}
static const pairs_sink_vtbl_t MEM_SINK_VTBL = {
    mem_begin_round, mem_emit_front_pair, mem_emit_spill_pair,
    mem_end_round,   mem_as_source,       mem_destroy};
static pairs_sink_t* make_mem_sink(void) {
  mem_sink_t* M = (mem_sink_t*)calloc(1, sizeof(mem_sink_t));
  if (!M) {
    fprintf(stderr, "make_mem_sink\n");
    exit(1);
  }
  M->base.v = &MEM_SINK_VTBL;
  return (pairs_sink_t*)M;
}

/*------------------------------------------------------------------------------
 * FileSink (sequential append + slice-mmap)
 *------------------------------------------------------------------------------*/
typedef struct {
  pairs_sink_t base;
  int fd;
  size_t front_chunk_pairs; /* default set by config */

  off_t offsets[LGK];
  uint32_t lens[LGK];

  ipair24_t** spill_vec;
  size_t *spill_sz, *spill_cap;

  ipair24_t* front_buf;
  size_t front_fill;
} file_sink_t;

static void file_flush_front(file_sink_t* F) {
  if (!F->front_fill) return;
  size_t bytes = F->front_fill * sizeof(ipair24_t);
  ssize_t w = write(F->fd, F->front_buf, bytes);
  if (w < 0 || (size_t)w != bytes) {
    perror("write(front)");
    exit(1);
  }
  F->front_fill = 0;
}

static void* file_begin_round(pairs_sink_t* s, int round_idx,
                              uint64_t expected_pairs) {
  file_sink_t* F = (file_sink_t*)s;
  off_t base = lseek(F->fd, 0, SEEK_END);
  if (base == (off_t)-1) {
    perror("lseek");
    exit(1);
  }
  F->offsets[round_idx] = base;
  F->lens[round_idx] = 0;
  F->front_fill = 0;

  size_t bytes = (size_t)expected_pairs * sizeof(ipair24_t);
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)
  if (bytes) {
    (void)posix_fallocate(F->fd, base, (off_t)bytes);
  }
#endif

  if (!F->spill_vec) {
    F->spill_vec = (ipair24_t**)calloc(LGK, sizeof(ipair24_t*));
    F->spill_sz = (size_t*)calloc(LGK, sizeof(size_t));
    F->spill_cap = (size_t*)calloc(LGK, sizeof(size_t));
    if (!F->spill_vec || !F->spill_sz || !F->spill_cap) {
      fprintf(stderr, "file spill alloc\n");
      exit(1);
    }
  }
  return (void*)(intptr_t)round_idx;
}
static void file_emit_front_pair(pairs_sink_t* s, void* ctx, uint32_t ia,
                                 uint32_t ib) {
  (void)ctx;
  file_sink_t* F = (file_sink_t*)s;
  if (!F->front_buf) {
    size_t pcs = F->front_chunk_pairs ? F->front_chunk_pairs : (size_t)131072;
    F->front_buf = (ipair24_t*)malloc(pcs * sizeof(ipair24_t));
    if (!F->front_buf) {
      fprintf(stderr, "front_buf alloc\n");
      exit(1);
    }
  }
  size_t cap = F->front_chunk_pairs ? F->front_chunk_pairs : (size_t)131072;
  ipair24_t* p = &F->front_buf[F->front_fill++];
  store_u24_le(p->a, ia);
  store_u24_le(p->b, ib);
  if (F->front_fill == cap) file_flush_front(F);
}
static void file_emit_spill_pair(pairs_sink_t* s, void* ctx, uint32_t ia,
                                 uint32_t ib) {
  file_sink_t* F = (file_sink_t*)s;
  int r = (int)(intptr_t)ctx;
  size_t sz = F->spill_sz[r], cap = F->spill_cap[r];
  if (sz == cap) {
    size_t nc = cap ? (cap * 3 / 2 + (1 << 10)) : (size_t)1 << 16;
    ipair24_t* np =
        (ipair24_t*)realloc(F->spill_vec[r], nc * sizeof(ipair24_t));
    if (!np) {
      fprintf(stderr, "file spill realloc\n");
      exit(1);
    }
    F->spill_vec[r] = np;
    F->spill_cap[r] = nc;
  }
  ipair24_t* p = &F->spill_vec[r][F->spill_sz[r]++];
  store_u24_le(p->a, ia);
  store_u24_le(p->b, ib);
}
static uint64_t file_end_round(pairs_sink_t* s, void* ctx) {
  file_sink_t* F = (file_sink_t*)s;
  int r = (int)(intptr_t)ctx;

  /* I/O advice (sequential) */
#if defined(POSIX_FADV_SEQUENTIAL) && defined(POSIX_FADV_NOREUSE)
  (void)posix_fadvise(F->fd, F->offsets[r], 0,
                      POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE);
#endif

  file_flush_front(F);
  if (F->spill_sz[r]) {
    size_t bytes = F->spill_sz[r] * sizeof(ipair24_t);
    ssize_t w = write(F->fd, F->spill_vec[r], bytes);
    if (w < 0 || (size_t)w != bytes) {
      perror("write(spill)");
      exit(1);
    }
    free(F->spill_vec[r]);
    F->spill_vec[r] = NULL;
    F->spill_sz[r] = F->spill_cap[r] = 0;
  }
  off_t end = lseek(F->fd, 0, SEEK_END);
  if (end == (off_t)-1) {
    perror("lseek end");
    exit(1);
  }
  off_t base = F->offsets[r];
  off_t diff = end - base;
  if (diff % (off_t)sizeof(ipair24_t)) {
    fprintf(stderr, "file_end: misalign\n");
    exit(1);
  }
  F->lens[r] = (uint32_t)(diff / (off_t)sizeof(ipair24_t));
  return F->lens[r];
}

/* file source (slice-mmap) */
typedef struct {
  pairs_source_t base;
  int fd;
  uint32_t len;
  void* map_base;
  size_t map_len;
  size_t delta;
  const ipair24_t* pairs;
} file_source_t;

static uint32_t file_src_length(pairs_source_t* src) {
  return ((file_source_t*)src)->len;
}
static void file_src_read(pairs_source_t* src, uint32_t idx, uint32_t* a,
                          uint32_t* b) {
  file_source_t* S = (file_source_t*)src;
  assert(idx < S->len);
  const ipair24_t* p = &S->pairs[idx];
  *a = load_u24_le(p->a);
  *b = load_u24_le(p->b);
}
static void file_src_close(pairs_source_t* src) {
  file_source_t* S = (file_source_t*)src;
  if (S->map_base && S->map_len) munmap(S->map_base, S->map_len);
  free(S);
}
static const pairs_source_vtbl_t FILE_SRC_VTBL = {
    file_src_length, file_src_read, file_src_close};

static pairs_source_t* file_as_source(pairs_sink_t* s, int round_idx) {
  file_sink_t* F = (file_sink_t*)s;
  size_t page = (size_t)sysconf(_SC_PAGESIZE);
  off_t base = F->offsets[round_idx];
  size_t bytes = (size_t)F->lens[round_idx] * sizeof(ipair24_t);
  file_source_t* S = (file_source_t*)calloc(1, sizeof(file_source_t));
  if (!S) {
    fprintf(stderr, "file_as_source malloc\n");
    exit(1);
  }
  S->base.v = &FILE_SRC_VTBL;
  S->fd = F->fd;
  S->len = F->lens[round_idx];
  if (!bytes) return (pairs_source_t*)S;
  size_t map_off = (size_t)(base & ~(off_t)(page - 1));
  size_t delta = (size_t)(base - (off_t)map_off);
  size_t map_len = delta + bytes;
  void* addr =
      mmap(NULL, map_len, PROT_READ, MAP_PRIVATE, F->fd, (off_t)map_off);
  if (addr == MAP_FAILED) {
    perror("mmap file slice");
    exit(1);
  }
  S->map_base = addr;
  S->map_len = map_len;
  S->delta = delta;
  S->pairs = (const ipair24_t*)((const uint8_t*)addr + delta);

#if defined(MADV_SEQUENTIAL) && defined(MADV_WILLNEED)
  (void)!madvise(S->map_base, S->map_len, MADV_SEQUENTIAL);
  (void)!madvise(S->map_base, S->map_len, MADV_WILLNEED);
#endif
  return (pairs_source_t*)S;
}
static void file_destroy(pairs_sink_t* s) {
  file_sink_t* F = (file_sink_t*)s;
  if (F->front_buf) free(F->front_buf);
  if (F->spill_vec) {
    for (int r = 0; r < LGK; ++r) free(F->spill_vec[r]);
    free(F->spill_vec);
    free(F->spill_sz);
    free(F->spill_cap);
  }
  if (F->fd >= 0) close(F->fd);
  free(F);
}
static const pairs_sink_vtbl_t FILE_SINK_VTBL = {
    file_begin_round, file_emit_front_pair, file_emit_spill_pair,
    file_end_round,   file_as_source,       file_destroy};
static pairs_sink_t* make_file_sink(const char* path,
                                    size_t front_chunk_pairs) {
  int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    perror("open file");
    exit(1);
  }
  file_sink_t* F = (file_sink_t*)calloc(1, sizeof(file_sink_t));
  if (!F) {
    fprintf(stderr, "make_file_sink\n");
    exit(1);
  }
  F->base.v = &FILE_SINK_VTBL;
  F->fd = fd;
  F->front_chunk_pairs = front_chunk_pairs;
  return (pairs_sink_t*)F;
}

/*------------------------------------------------------------------------------
 * ComboSink (route per-round to MEM/FILE)
 *------------------------------------------------------------------------------*/
typedef enum { BACKEND_MEM, BACKEND_FILE } backend_t;
typedef struct {
  pairs_sink_t base;
  pairs_sink_t* mem;
  pairs_sink_t* file;
  backend_t be[LGK];
} combo_sink_t;
typedef struct {
  backend_t be;
  void* inner;
  int round;
} combo_ctx_t;

static void* combo_begin_round(pairs_sink_t* s, int round_idx,
                               uint64_t expected_pairs) {
  combo_sink_t* C = (combo_sink_t*)s;
  backend_t be = C->be[round_idx];
  void* inner =
      (be == BACKEND_MEM)
          ? C->mem->v->begin_round(C->mem, round_idx, expected_pairs)
          : C->file->v->begin_round(C->file, round_idx, expected_pairs);
  combo_ctx_t* ctx = (combo_ctx_t*)malloc(sizeof(combo_ctx_t));
  if (!ctx) {
    fprintf(stderr, "combo ctx alloc\n");
    exit(1);
  }
  ctx->be = be;
  ctx->inner = inner;
  ctx->round = round_idx;
  return ctx;
}
static void combo_emit_front_pair(pairs_sink_t* s, void* ctx_, uint32_t ia,
                                  uint32_t ib) {
  combo_sink_t* C = (combo_sink_t*)s;
  combo_ctx_t* ctx = (combo_ctx_t*)ctx_;
  if (ctx->be == BACKEND_MEM)
    C->mem->v->emit_front_pair(C->mem, ctx->inner, ia, ib);
  else
    C->file->v->emit_front_pair(C->file, ctx->inner, ia, ib);
}
static void combo_emit_spill_pair(pairs_sink_t* s, void* ctx_, uint32_t ia,
                                  uint32_t ib) {
  combo_sink_t* C = (combo_sink_t*)s;
  combo_ctx_t* ctx = (combo_ctx_t*)ctx_;
  if (ctx->be == BACKEND_MEM)
    C->mem->v->emit_spill_pair(C->mem, ctx->inner, ia, ib);
  else
    C->file->v->emit_spill_pair(C->file, ctx->inner, ia, ib);
}
static uint64_t combo_end_round(pairs_sink_t* s, void* ctx_) {
  combo_sink_t* C = (combo_sink_t*)s;
  combo_ctx_t* ctx = (combo_ctx_t*)ctx_;
  uint64_t r = (ctx->be == BACKEND_MEM)
                   ? C->mem->v->end_round(C->mem, ctx->inner)
                   : C->file->v->end_round(C->file, ctx->inner);
  free(ctx);
  return r;
}
static pairs_source_t* combo_as_source(pairs_sink_t* s, int round_idx) {
  combo_sink_t* C = (combo_sink_t*)s;
  return (C->be[round_idx] == BACKEND_MEM)
             ? C->mem->v->as_source(C->mem, round_idx)
             : C->file->v->as_source(C->file, round_idx);
}
static void combo_destroy(pairs_sink_t* s) {
  combo_sink_t* C = (combo_sink_t*)s;
  if (C->mem) C->mem->v->destroy(C->mem);
  if (C->file) C->file->v->destroy(C->file);
  free(C);
}
static const pairs_sink_vtbl_t COMBO_VTBL = {
    combo_begin_round, combo_emit_front_pair, combo_emit_spill_pair,
    combo_end_round,   combo_as_source,       combo_destroy};
static pairs_sink_t* make_combo_sink(backend_t per_round[LGK],
                                     const char* filepath,
                                     size_t front_chunk_pairs) {
  combo_sink_t* C = (combo_sink_t*)calloc(1, sizeof(combo_sink_t));
  if (!C) {
    fprintf(stderr, "make_combo_sink\n");
    exit(1);
  }
  C->base.v = &COMBO_VTBL;
  for (int i = 0; i < LGK; ++i) C->be[i] = per_round[i];
  C->mem = make_mem_sink();
  if (filepath && *filepath)
    C->file = make_file_sink(filepath, front_chunk_pairs);
  else {
    for (int i = 0; i < LGK; ++i)
      if (per_round[i] == BACKEND_FILE) {
        fprintf(stderr, "file backend requested but no path\n");
        exit(1);
      }
  }
  return (pairs_sink_t*)C;
}

/*------------------------------------------------------------------------------
 * Merge operators
 *------------------------------------------------------------------------------*/
static int cmp_u32pair(const void* a, const void* b) {
  const uint32_t *pa = (const uint32_t*)a, *pb = (const uint32_t*)b;
  if (pa[0] < pb[0]) return -1;
  if (pa[0] > pb[0]) return +1;
  return 0;
}

/* values-only (optionally produce phys->enum as u24 packed in uint32_t buf) */
static void merge20_values_only(stage_list_t* cur, uint8_t** p_phys2enum_u24) {
  const uint8_t in_bytes = cur->item_bytes;
  const uint8_t out_bytes = (uint8_t)(((in_bytes * 8u - 20u) + 7u) / 8u);

  reorder_into_bucket_order(cur, NULL);
  const uint32_t B = 1u << ELL;

  /* first pass count */
  uint64_t out_cnt = 0;
  for (uint32_t k = 0; k < B; ++k) {
    uint32_t m = CNT[k];
    if (m < 2) continue;
    uint32_t s = OFFS[k], e = s + m;
    for (uint32_t a = s; a < e; ++a) {
      const uint8_t* A = &cur->buf[(size_t)a * in_bytes];
      for (uint32_t b = s; b < a; ++b) {
        const uint8_t* Bv = &cur->buf[(size_t)b * in_bytes];
        /* reject all-zero */
        uint8_t t[32];
        xor_shr20_store(t, A, Bv, in_bytes, out_bytes);
        int all0 = 1;
        for (uint8_t z = 0; z < out_bytes; ++z) {
          if (t[z]) {
            all0 = 0;
            break;
          }
        }
        if (!all0) out_cnt++;
      }
    }
  }

  uint8_t *phys2enum_u24 = NULL, *spill_enum_u24 = NULL;
  size_t spill_enum_sz = 0;
  if (p_phys2enum_u24 && out_cnt) {
    phys2enum_u24 = (uint8_t*)malloc((size_t)out_cnt * 3);
    spill_enum_u24 = (uint8_t*)malloc((size_t)out_cnt * 3);
    if (!phys2enum_u24 || !spill_enum_u24) {
      fprintf(stderr, "phys2enum_u24 alloc\n");
      exit(1);
    }
  }

  spill_t sp;
  spill_init(&sp, 8u << 20);
  size_t W = 0, S = 0;
  uint64_t written = 0, enum_idx = 0;

  for (uint32_t k = 0; k < B; ++k) {
    uint32_t m = CNT[k];
    if (m < 2) {
      S += (size_t)m * in_bytes;
      continue;
    }
    uint32_t s = OFFS[k], e = s + m;
    for (uint32_t a = s; a < e; ++a) {
      const uint8_t* A = &cur->buf[(size_t)a * in_bytes];
      for (uint32_t b = s; b < a; ++b) {
        const uint8_t* Bv = &cur->buf[(size_t)b * in_bytes];
        uint8_t t[32];
        xor_shr20_store(t, A, Bv, in_bytes, out_bytes);
        int all0 = 1;
        for (uint8_t z = 0; z < out_bytes; ++z) {
          if (t[z]) {
            all0 = 0;
            break;
          }
        }
        if (all0) continue;
        if (S >= out_bytes) {
          memcpy(cur->buf + W, t, out_bytes);
          if (phys2enum_u24) {
            store_u24_le(&phys2enum_u24[(W / out_bytes) * 3],
                         (uint32_t)enum_idx);
          }
          W += out_bytes;
          S -= out_bytes;
        } else {
          spill_append(&sp, t, out_bytes);
          if (spill_enum_u24) {
            store_u24_le(&spill_enum_u24[spill_enum_sz * 3],
                         (uint32_t)enum_idx);
            spill_enum_sz++;
          }
        }
        written++;
        enum_idx++;
      }
    }
    S += (size_t)m * in_bytes;
  }

  size_t front_cnt = W / out_bytes;
  if (sp.size) memcpy(cur->buf + W, sp.buf, sp.size);
  if (phys2enum_u24 && spill_enum_u24) {
    for (size_t i = 0; i < spill_enum_sz; ++i) {
      memcpy(&phys2enum_u24[(front_cnt + i) * 3], &spill_enum_u24[i * 3], 3);
    }
    free(spill_enum_u24);
  }
  spill_free(&sp);

  cur->len = (uint32_t)written;
  cur->item_bytes = out_bytes;
  if (p_phys2enum_u24) *p_phys2enum_u24 = phys2enum_u24;
}

/* values + emit pairs to sink */
static void merge20_values_plus_pairs(stage_list_t* cur, pairs_sink_t* sink,
                                      int round_idx) {
  const uint8_t in_bytes = cur->item_bytes;
  const uint8_t out_bytes = (uint8_t)(((in_bytes * 8u - 20u) + 7u) / 8u);

  uint32_t* idmap = (uint32_t*)malloc(sizeof(uint32_t) * cur->len);
  if (!idmap && cur->len) {
    fprintf(stderr, "idmap alloc\n");
    exit(1);
  }
  for (uint32_t i = 0; i < cur->len; ++i) idmap[i] = i;

  reorder_into_bucket_order(cur, idmap);
  const uint32_t B = 1u << ELL;

  uint64_t cap_pairs = estimate_pairs_upper_bound_from_cnt();
  void* ctx = sink->v->begin_round(sink, round_idx, cap_pairs);

  spill_t sp;
  spill_init(&sp, 8u << 20);
  size_t W = 0, S = 0;

  for (uint32_t k = 0; k < B; ++k) {
    uint32_t m = CNT[k];
    if (m < 2) {
      S += (size_t)m * in_bytes;
      continue;
    }
    uint32_t s = OFFS[k], e = s + m;
    for (uint32_t a = s; a < e; ++a) {
      const uint8_t* A = &cur->buf[(size_t)a * in_bytes];
      uint32_t ea = idmap[a];
      for (uint32_t b = s; b < a; ++b) {
        const uint8_t* Bv = &cur->buf[(size_t)b * in_bytes];
        uint32_t eb = idmap[b];
        uint8_t t[32];
        xor_shr20_store(t, A, Bv, in_bytes, out_bytes);
        int all0 = 1;
        for (uint8_t z = 0; z < out_bytes; ++z) {
          if (t[z]) {
            all0 = 0;
            break;
          }
        }
        if (all0) continue;

        if (S >= out_bytes) {
          memcpy(cur->buf + W, t, out_bytes);
          sink->v->emit_front_pair(sink, ctx, ea, eb);
          W += out_bytes;
          S -= out_bytes;
        } else {
          spill_append(&sp, t, out_bytes);
          sink->v->emit_spill_pair(sink, ctx, ea, eb);
        }
      }
    }
    S += (size_t)m * in_bytes;
  }

  if (sp.size) memcpy(cur->buf + W, sp.buf, sp.size);
  spill_free(&sp);
  free(idmap);

  uint64_t pairs = sink->v->end_round(sink, ctx);
  cur->len = (uint32_t)pairs;
  cur->item_bytes = out_bytes;
}

/* pairs-only two-pass (count then emit), values untouched */
static void merge20_pairs_only_emit(const stage_list_t* cur, pairs_sink_t* sink,
                                    int round_idx) {
  const uint8_t in_bytes = cur->item_bytes;
  const uint32_t B = 1u << ELL;

  ensure_workspace(cur->len);
  memset(CNT, 0, (size_t)B * sizeof(uint32_t));
  for (uint32_t i = 0; i < cur->len; ++i) {
    uint32_t key = low20_from_le(&cur->buf[(size_t)i * in_bytes]);
    CNT[key]++;
  }
  uint64_t total = 0;
  for (uint32_t k = 0; k < B; ++k) {
    OFFS[k] = (uint32_t)total;
    total += CNT[k];
  }
  for (uint32_t i = 0; i < cur->len; ++i) {
    uint32_t key = low20_from_le(&cur->buf[(size_t)i * in_bytes]);
    ORDER[OFFS[key]++] = i;
  }
  for (uint32_t k = B; k-- > 0;) OFFS[k] = (k == 0 ? 0 : OFFS[k - 1]);

  uint64_t pairs = 0;
  uint8_t out_bytes = (uint8_t)(((in_bytes * 8u - 20u) + 7u) / 8u);

  /* pass 1: count */
  for (uint32_t k = 0; k < B; ++k) {
    uint32_t m = CNT[k];
    if (m < 2) continue;
    uint32_t s = OFFS[k];
    for (uint32_t a = s; a < s + m; ++a) {
      uint32_t ia = ORDER[a];
      const uint8_t* A = &cur->buf[(size_t)ia * in_bytes];
      for (uint32_t b = s; b < a; ++b) {
        uint32_t ib = ORDER[b];
        const uint8_t* Bv = &cur->buf[(size_t)ib * in_bytes];
        uint8_t t[32];
        xor_shr20_store(t, A, Bv, in_bytes, out_bytes);
        int all0 = 1;
        for (uint8_t z = 0; z < out_bytes; ++z) {
          if (t[z]) {
            all0 = 0;
            break;
          }
        }
        if (!all0) pairs++;
      }
    }
  }

  void* ctx = sink->v->begin_round(sink, round_idx, pairs);

  /* pass 2: emit */
  for (uint32_t k = 0; k < B; ++k) {
    uint32_t m = CNT[k];
    if (m < 2) continue;
    uint32_t s = OFFS[k];
    for (uint32_t a = s; a < s + m; ++a) {
      uint32_t ia = ORDER[a];
      const uint8_t* A = &cur->buf[(size_t)ia * in_bytes];
      for (uint32_t b = s; b < a; ++b) {
        uint32_t ib = ORDER[b];
        const uint8_t* Bv = &cur->buf[(size_t)ib * in_bytes];
        uint8_t t[32];
        xor_shr20_store(t, A, Bv, in_bytes, out_bytes);
        int all0 = 1;
        for (uint8_t z = 0; z < out_bytes; ++z) {
          if (t[z]) {
            all0 = 0;
            break;
          }
        }
        if (all0) continue;
        sink->v->emit_front_pair(sink, ctx, ia, ib);
      }
    }
  }
  sink->v->end_round(sink, ctx);
}

/* final 40-bit: group by high20 of low40 inside each 20-bit bucket */
static void merge40_pairs_only_emit(const stage_list_t* L, pairs_sink_t* sink,
                                    int round_idx) {
  const uint8_t in_bytes = L->item_bytes;
  const uint32_t B = 1u << ELL;

  ensure_workspace(L->len);
  memset(CNT, 0, (size_t)B * sizeof(uint32_t));
  for (uint32_t i = 0; i < L->len; ++i) {
    uint32_t key = low20_from_le(&L->buf[(size_t)i * in_bytes]);
    CNT[key]++;
  }
  uint64_t total = 0;
  for (uint32_t k = 0; k < B; ++k) {
    OFFS[k] = (uint32_t)total;
    total += CNT[k];
  }
  for (uint32_t i = 0; i < L->len; ++i) {
    uint32_t key = low20_from_le(&L->buf[(size_t)i * in_bytes]);
    ORDER[OFFS[key]++] = i;
  }
  for (uint32_t k = B; k-- > 0;) OFFS[k] = (k == 0 ? 0 : OFFS[k - 1]);

  uint32_t max_m = 0;
  for (uint32_t k = 0; k < B; ++k)
    if (CNT[k] > max_m) max_m = CNT[k];
  uint32_t* tmp = (uint32_t*)malloc((size_t)max_m * 2 * sizeof(uint32_t));
  if (!tmp && max_m) {
    fprintf(stderr, "merge40 tmp alloc\n");
    exit(1);
  }

  /* count */
  uint64_t pairs = 0;
  for (uint32_t k = 0; k < B; ++k) {
    uint32_t m = CNT[k];
    if (m < 2) continue;
    uint32_t s = OFFS[k];
    for (uint32_t t = 0; t < m; ++t) {
      uint32_t i = ORDER[s + t];
      const uint8_t* P = &L->buf[(size_t)i * in_bytes];
      /* low40: first 5 bytes, LE; high20 is bits 20..39 => bytes 3..4 in LE
       * view */
      uint64_t w = (uint64_t)P[0] | ((uint64_t)P[1] << 8) |
                   ((uint64_t)P[2] << 16) | ((uint64_t)P[3] << 24) |
                   ((uint64_t)P[4] << 32);
      uint32_t hi20 = (uint32_t)((w >> 20) & 0xFFFFFu);
      tmp[2 * t] = hi20;
      tmp[2 * t + 1] = i;
    }
    qsort(tmp, m, sizeof(uint32_t) * 2, cmp_u32pair);
    uint32_t run = 1;
    for (uint32_t t = 1; t <= m; ++t) {
      if (t < m && tmp[2 * t] == tmp[2 * (t - 1)])
        run++;
      else {
        if (run >= 2) pairs += (uint64_t)run * (run - 1) / 2;
        run = 1;
      }
    }
  }

  void* ctx = sink->v->begin_round(sink, round_idx, pairs);

  /* emit */
  for (uint32_t k = 0; k < B; ++k) {
    uint32_t m = CNT[k];
    if (m < 2) continue;
    uint32_t s = OFFS[k];
    for (uint32_t t = 0; t < m; ++t) {
      uint32_t i = ORDER[s + t];
      const uint8_t* P = &L->buf[(size_t)i * in_bytes];
      uint64_t w = (uint64_t)P[0] | ((uint64_t)P[1] << 8) |
                   ((uint64_t)P[2] << 16) | ((uint64_t)P[3] << 24) |
                   ((uint64_t)P[4] << 32);
      uint32_t hi20 = (uint32_t)((w >> 20) & 0xFFFFFu);
      tmp[2 * t] = hi20;
      tmp[2 * t + 1] = i;
    }
    qsort(tmp, m, sizeof(uint32_t) * 2, cmp_u32pair);
    uint32_t run_s = 0;
    while (run_s < m) {
      uint32_t run_e = run_s + 1;
      while (run_e < m && tmp[2 * run_e] == tmp[2 * run_s]) run_e++;
      uint32_t run = run_e - run_s;
      if (run >= 2) {
        for (uint32_t a = run_s + 1; a < run_e; ++a) {
          uint32_t ia = tmp[2 * a + 1];
          for (uint32_t b = run_s; b < a; ++b) {
            uint32_t ib = tmp[2 * b + 1];
            sink->v->emit_front_pair(sink, ctx, ia, ib);
          }
        }
      }
      run_s = run_e;
    }
  }
  sink->v->end_round(sink, ctx);
  free(tmp);
}

/*------------------------------------------------------------------------------
 * Backtracking
 *------------------------------------------------------------------------------*/
typedef struct {
  uint32_t* idx;
  uint32_t len;
} idxvec_t;

static int _check_valid_index_vector(const uint32_t* vec, uint32_t len,
                                     uint32_t** real_vec, uint32_t* real_len) {
  const uint32_t MAPSZ = N;
  uint32_t words = (MAPSZ + 31u) / 32u;
  uint32_t* bitmap = (uint32_t*)calloc(words, sizeof(uint32_t));
  if (!bitmap) {
    fprintf(stderr, "bitmap alloc\n");
    exit(1);
  }
  for (uint32_t i = 0; i < len; ++i) {
    uint32_t x = vec[i];
    uint32_t w = x >> 5, b = x & 31u;
    bitmap[w] ^= (1u << b);
  }
  uint32_t* rv = (uint32_t*)malloc(sizeof(uint32_t) * len);
  uint32_t rlen = 0;
  for (uint32_t w = 0; w < words; ++w) {
    uint32_t v = bitmap[w];
    while (v) {
      uint32_t b = __builtin_ctz(v);
      v &= v - 1;
      rv[rlen++] = (w << 5) | b;
    }
  }
  free(bitmap);
  if (rlen == 0) {
    free(rv);
    *real_vec = NULL;
    *real_len = 0;
    return 0;
  } else if (rlen == (1u << LGK)) {
    *real_vec = rv;
    *real_len = rlen;
    return 1;
  } else {
    *real_vec = rv;
    *real_len = rlen;
    return 2;
  }
}

static void verify_results(const uint32_t* indices, uint32_t len,
                           const uint8_t* nonce) {
  uint8_t acc[OUT_BYTES0];
  memset(acc, 0, OUT_BYTES0);
  blake2b_state base;
  blake2b_init(&base, OUT_BYTES0);
  blake2b_update(&base, nonce, 16);
  for (uint32_t i = 0; i < len; ++i) {
    blake2b_state S = base;
    uint32_t ctr = indices[i];
    blake2b_update(&S, (uint8_t*)&ctr, sizeof(ctr));
    uint8_t h[OUT_BYTES0];
    blake2b_final(&S, h, OUT_BYTES0);
    for (int b = 0; b < OUT_BYTES0; ++b) acc[b] ^= h[b];
  }
  int all0 = 1;
  for (int b = 0; b < OUT_BYTES0; ++b)
    if (acc[b]) {
      all0 = 0;
      break;
    }
  if (!all0) {
    fprintf(stderr, "verify failed\n");
    exit(1);
  }
}

static void backtrack_plain_or_em(pairs_sink_t* sink, const uint8_t* nonce) {
  pairs_source_t* last = sink->v->as_source(sink, LGK - 1);
  uint32_t sol_cnt = last->v->length(last);
  idxvec_t* solutions = (idxvec_t*)malloc(sizeof(idxvec_t) * sol_cnt);
  if (!solutions && sol_cnt) {
    fprintf(stderr, "solutions alloc\n");
    exit(1);
  }
  for (uint32_t t = 0; t < sol_cnt; ++t) {
    solutions[t].len = 2;
    solutions[t].idx = (uint32_t*)malloc(sizeof(uint32_t) * 2);
    uint32_t a, b;
    last->v->read_pair(last, t, &a, &b);
    solutions[t].idx[0] = a;
    solutions[t].idx[1] = b;
  }
  last->v->close(last);

  for (int layer = LGK - 2; layer >= 0; --layer) {
    pairs_source_t* S = sink->v->as_source(sink, layer);
    uint32_t pairs_len = S->v->length(S);
    (void)pairs_len;
    idxvec_t* ext = (idxvec_t*)malloc(sizeof(idxvec_t) * sol_cnt);
    if (!ext && sol_cnt) {
      fprintf(stderr, "ext alloc\n");
      exit(1);
    }
    for (uint32_t s = 0; s < sol_cnt; ++s) {
      const idxvec_t* sv = &solutions[s];
      idxvec_t* ev = &ext[s];
      ev->len = sv->len * 2;
      ev->idx = (uint32_t*)malloc(sizeof(uint32_t) * ev->len);
      uint32_t w = 0;
      for (uint32_t i = 0; i < sv->len; ++i) {
        uint32_t idx = sv->idx[i];
        assert(idx < pairs_len);
        uint32_t a, b;
        S->v->read_pair(S, idx, &a, &b);
        ev->idx[w++] = a;
        ev->idx[w++] = b;
      }
    }
    S->v->close(S);
    for (uint32_t s = 0; s < sol_cnt; ++s) free(solutions[s].idx);
    free(solutions);
    solutions = ext;
  }

  uint32_t solutions_found = 0, perfect = 0, secondary = 0, trivial = 0;
  for (uint32_t s = 0; s < sol_cnt; ++s) {
    uint32_t *real = NULL, rlen = 0;
    int tp = _check_valid_index_vector(solutions[s].idx, solutions[s].len,
                                       &real, &rlen);
    if (tp == 0) {
      trivial++;
      free(real);
      continue;
    }
    verify_results(real, rlen, nonce);
    solutions_found++;
    if (tp == 1)
      perfect++;
    else
      secondary++;
    free(real);
  }
  printf("[results] total=%u, perfect=%u, secondary=%u, trivial=%u\n",
         solutions_found, perfect, secondary, trivial);
  for (uint32_t s = 0; s < sol_cnt; ++s) free(solutions[s].idx);
  free(solutions);
}

/*------------------------------------------------------------------------------
 * Strategies
 *------------------------------------------------------------------------------*/
typedef enum { STRAT_PLAIN_IP, STRAT_IP_PR, STRAT_IP_EM } strategy_t;

typedef struct {
  strategy_t strat;
  backend_t per_round_backend[LGK];
  char file_path[512];
  size_t front_pairs_chunk; /* default 131072 */
  uint8_t nonce[16];
} run_config_t;

/* plain_ip: 8×20 values+pairs, final 40 pairs */
static void run_plain_ip(const run_config_t* C) {
  backend_t BE[LGK];
  for (int i = 0; i < LGK; ++i) BE[i] = BACKEND_MEM;
  pairs_sink_t* sink = make_combo_sink(BE, NULL, C->front_pairs_chunk);

  stage_list_t cur = {0};
  compute_hash_list(&cur, C->nonce);
  for (int r = 0; r < LGK - 1; ++r) merge20_values_plus_pairs(&cur, sink, r);
  merge40_pairs_only_emit(&cur, sink, LGK - 1);
  free(cur.buf);

  backtrack_plain_or_em(sink, C->nonce);
  sink->v->destroy(sink);
  free_workspace();
}

/* ip_pr: post-retrieval; per round: rebuild values-only to r-1, build
 * phys->enum(u24), emit pairs-only on r */
static void run_ip_pr(const run_config_t* C) {
  idxvec_t* solutions = NULL;
  uint32_t sol_cnt = 0;

  for (int n_round = LGK; n_round >= 1; --n_round) {
    backend_t BE_once[LGK];
    for (int i = 0; i < LGK; ++i) BE_once[i] = BACKEND_MEM;
    pairs_sink_t* sink_once =
        make_combo_sink(BE_once, NULL, C->front_pairs_chunk);

    stage_list_t cur = {0};
    compute_hash_list(&cur, C->nonce);

    uint8_t* phys2enum_u24 = NULL;
    for (int i = 0; i < n_round - 1; ++i) {
      if (i == n_round - 2)
        merge20_values_only(&cur, &phys2enum_u24);
      else
        merge20_values_only(&cur, NULL);
    }
    if (!phys2enum_u24) { /* n_round==1: identity */
      phys2enum_u24 = (uint8_t*)malloc((size_t)cur.len * 3);
      if (!phys2enum_u24 && cur.len) {
        fprintf(stderr, "phys2enum id alloc\n");
        exit(1);
      }
      for (uint32_t i = 0; i < cur.len; ++i)
        store_u24_le(&phys2enum_u24[i * 3], i);
    }

    if (n_round == LGK)
      merge40_pairs_only_emit(&cur, sink_once, n_round - 1);
    else
      merge20_pairs_only_emit(&cur, sink_once, n_round - 1);
    free(cur.buf);

    pairs_source_t* S = sink_once->v->as_source(sink_once, n_round - 1);
    uint32_t pairs_len = S->v->length(S);

    if (n_round == LGK) {
      sol_cnt = pairs_len;
      solutions = (idxvec_t*)malloc(sizeof(idxvec_t) * sol_cnt);
      if (!solutions && sol_cnt) {
        fprintf(stderr, "solutions alloc\n");
        exit(1);
      }
      for (uint32_t t = 0; t < sol_cnt; ++t) {
        uint32_t a, b;
        S->v->read_pair(S, t, &a, &b);
        solutions[t].len = 2;
        solutions[t].idx = (uint32_t*)malloc(sizeof(uint32_t) * 2);
        solutions[t].idx[0] = load_u24_le(&phys2enum_u24[a * 3]);
        solutions[t].idx[1] = load_u24_le(&phys2enum_u24[b * 3]);
      }
    } else {
      idxvec_t* ext = (idxvec_t*)malloc(sizeof(idxvec_t) * sol_cnt);
      if (!ext && sol_cnt) {
        fprintf(stderr, "ext alloc\n");
        exit(1);
      }
      for (uint32_t s = 0; s < sol_cnt; ++s) {
        const idxvec_t* sv = &solutions[s];
        idxvec_t* ev = &ext[s];
        ev->len = sv->len * 2;
        ev->idx = (uint32_t*)malloc(sizeof(uint32_t) * ev->len);
        uint32_t w = 0;
        for (uint32_t i = 0; i < sv->len; ++i) {
          uint32_t parent = sv->idx[i];
          assert(parent < pairs_len);
          uint32_t a, b;
          S->v->read_pair(S, parent, &a, &b);
          ev->idx[w++] = load_u24_le(&phys2enum_u24[a * 3]);
          ev->idx[w++] = load_u24_le(&phys2enum_u24[b * 3]);
        }
      }
      for (uint32_t s = 0; s < sol_cnt; ++s) free(solutions[s].idx);
      free(solutions);
      solutions = ext;
    }

    S->v->close(S);
    sink_once->v->destroy(sink_once);
    free(phys2enum_u24);
    printf("[ip_pr] round %d done.\n", LGK - n_round + 1);
  }

  uint32_t solutions_found = 0, perfect = 0, secondary = 0, trivial = 0;
  for (uint32_t s = 0; s < sol_cnt; ++s) {
    uint32_t *real = NULL, rlen = 0;
    int tp = _check_valid_index_vector(solutions[s].idx, solutions[s].len,
                                       &real, &rlen);
    if (tp == 0) {
      trivial++;
      free(real);
      continue;
    }
    verify_results(real, rlen, C->nonce);
    solutions_found++;
    if (tp == 1)
      perfect++;
    else
      secondary++;
    free(real);
  }
  printf("[ip_pr] Solutions: total=%u, perfect=%u, secondary=%u, trivial=%u\n",
         solutions_found, perfect, secondary, trivial);
  for (uint32_t s = 0; s < sol_cnt; ++s) free(solutions[s].idx);
  free(solutions);
  free_workspace();
}

/* ip_em: FILE for first 8 rounds, MEM for final; backtrack via slice-mmap */
static void run_ip_em(const run_config_t* C) {
  backend_t BE[LGK];
  for (int i = 0; i < LGK - 1; ++i) BE[i] = BACKEND_FILE;
  BE[LGK - 1] = BACKEND_MEM;
  pairs_sink_t* sink = make_combo_sink(BE, C->file_path, C->front_pairs_chunk);

  stage_list_t cur = {0};
  compute_hash_list(&cur, C->nonce);
  for (int r = 0; r < LGK - 1; ++r) merge20_values_plus_pairs(&cur, sink, r);
  merge40_pairs_only_emit(&cur, sink, LGK - 1);
  free(cur.buf);

  backtrack_plain_or_em(sink, C->nonce);
  sink->v->destroy(sink);
  free_workspace();
}

/*------------------------------------------------------------------------------
 * Bench harness
 *------------------------------------------------------------------------------*/
typedef struct {
  double sum, sum2, minv, maxv;
  uint32_t n;
} stats_t;
static void stats_init(stats_t* s) {
  s->sum = s->sum2 = 0.0;
  s->minv = 1e300;
  s->maxv = -1e300;
  s->n = 0;
}
static void stats_add(stats_t* s, double v) {
  s->sum += v;
  s->sum2 += v * v;
  s->n++;
  if (v < s->minv) s->minv = v;
  if (v > s->maxv) s->maxv = v;
}
static void stats_mean_std(const stats_t* s, double* mean, double* std) {
  if (!s->n) {
    *mean = *std = 0;
    return;
  }
  *mean = s->sum / s->n;
  double var = (s->sum2 / s->n) - (*mean) * (*mean);
  *std = var > 0 ? sqrt(var) : 0;
}

static int run_once_in_child(strategy_t strat, const char* file_path,
                             const uint8_t nonce16[16], double* time_s_out,
                             double* peak_mib_out) {
  double t0 = wall_now_sec();
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(1);
  }
  if (pid == 0) {
    run_config_t C;
    memset(&C, 0, sizeof(C));
    C.strat = strat;
    memcpy(C.nonce, nonce16, 16);
    C.front_pairs_chunk = 131072; /* unified large front buffer */
    if (strat == STRAT_IP_EM) {
      const char* path =
          (file_path && *file_path) ? file_path : "/var/tmp/ip_n200_k512.bin";
      strncpy(C.file_path, path, sizeof(C.file_path) - 1);
      C.file_path[sizeof(C.file_path) - 1] = '\0';
    }
    if (strat == STRAT_PLAIN_IP)
      run_plain_ip(&C);
    else if (strat == STRAT_IP_PR)
      run_ip_pr(&C);
    else
      run_ip_em(&C);
    _exit(0);
  }
  int status = 0;
  struct rusage ru;
  if (wait4(pid, &status, 0, &ru) < 0) {
    perror("wait4");
    exit(1);
  }
  double t1 = wall_now_sec();
  if (time_s_out) *time_s_out = t1 - t0;
  if (peak_mib_out) *peak_mib_out = rss_mib_from_rusage(&ru);
  return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 1 : 0;
}

typedef struct {
  int ok_count;
  int trials;
  stats_t time_s;
  stats_t peak_mib;
} bench_summary_t;

static bench_summary_t bench_strategy(strategy_t strat, int repeat,
                                      const char* file_path) {
  bench_summary_t R;
  R.ok_count = 0;
  R.trials = repeat;
  stats_init(&R.time_s);
  stats_init(&R.peak_mib);
  uint64_t seed = (uint64_t)time(NULL) ^ (uint64_t)getpid();
  for (int i = 0; i < repeat; ++i) {
    uint64_t x = seed + (uint64_t)(i * 0x9E3779B97F4A7C15ull);
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    x *= 0x2545F4914F6CDD1Dull;
    uint64_t y = x ^ (x << 7) ^ (x >> 3);
    uint8_t nonce16[16];
    memcpy(nonce16, &x, 8);
    memcpy(nonce16 + 8, &y, 8);
    double t = 0.0, m = 0.0;
    int ok = run_once_in_child(strat, file_path, nonce16, &t, &m);
    if (ok) {
      R.ok_count++;
      stats_add(&R.time_s, t);
      stats_add(&R.peak_mib, m);
    }
    printf("Nonce[%d]: %016llx%016llx\n", i, (unsigned long long)x,
           (unsigned long long)y);
  }
  return R;
}

static void bench_all(int repeat, const char* em_path_cli,
                      bench_summary_t* S_plain, bench_summary_t* S_pr,
                      bench_summary_t* S_em) {
  S_plain->ok_count = S_pr->ok_count = S_em->ok_count = 0;
  S_plain->trials = S_pr->trials = S_em->trials = repeat;
  stats_init(&S_plain->time_s);
  stats_init(&S_plain->peak_mib);
  stats_init(&S_pr->time_s);
  stats_init(&S_pr->peak_mib);
  stats_init(&S_em->time_s);
  stats_init(&S_em->peak_mib);

  uint64_t seed = (uint64_t)time(NULL) ^ (uint64_t)getpid();
  for (int i = 0; i < repeat; ++i) {
    uint64_t x = seed + (uint64_t)(i * 0x9E3779B97F4A7C15ull);
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    x *= 0x2545F4914F6CDD1Dull;
    uint64_t y = x ^ (x << 7) ^ (x >> 3);
    uint8_t nonce16[16];
    memcpy(nonce16, &x, 8);
    memcpy(nonce16 + 8, &y, 8);
    double t = 0.0, m = 0.0;

    if (run_once_in_child(STRAT_PLAIN_IP, NULL, nonce16, &t, &m)) {
      S_plain->ok_count++;
      stats_add(&S_plain->time_s, t);
      stats_add(&S_plain->peak_mib, m);
    }
    if (run_once_in_child(STRAT_IP_PR, NULL, nonce16, &t, &m)) {
      S_pr->ok_count++;
      stats_add(&S_pr->time_s, t);
      stats_add(&S_pr->peak_mib, m);
    }
    if (run_once_in_child(STRAT_IP_EM, em_path_cli, nonce16, &t, &m)) {
      S_em->ok_count++;
      stats_add(&S_em->time_s, t);
      stats_add(&S_em->peak_mib, m);
    }
  }
}

static void print_table_header(void) {
  printf("\n=== Benchmark Results ===\n");
  printf("%-10s %7s | %9s %7s %7s %7s | %13s %7s %7s %7s\n", "Algorithm",
         "ok/rep", "time_avg", "std", "min", "max", "peak_rss(MiB)", "std",
         "min", "max");
  printf(
      "------------------------------------------------------------------------"
      "---------------\n");
}
static void print_table_row(const char* name, const bench_summary_t* S) {
  double t_mean, t_std, m_mean, m_std;
  stats_mean_std(&S->time_s, &t_mean, &t_std);
  stats_mean_std(&S->peak_mib, &m_mean, &m_std);
  printf(
      "%-10s %3d/%-3d | %9.2f %7.2f %7.2f %7.2f | %13.1f %7.1f %7.1f %7.1f\n",
      name, S->ok_count, S->trials, t_mean, t_std, S->time_s.minv,
      S->time_s.maxv, m_mean, m_std, S->peak_mib.minv, S->peak_mib.maxv);
}

/*------------------------------------------------------------------------------
 * main
 *------------------------------------------------------------------------------*/
int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr,
            "Usage:\n"
            "  %s [plain_ip | ip_pr | ip_em] [external_path] [--repeat=R]\n"
            "  %s --all [external_path_for_ip_em] [--repeat=R]\n",
            argv[0], argv[0]);
    return 1;
  }
  int repeat = 1;
  const char* em_path_cli = NULL;
  int run_all = (strcmp(argv[1], "--all") == 0);
  strategy_t strat = (strategy_t)-1;

  if (!run_all) {
    strat = (strcmp(argv[1], "plain_ip") == 0) ? STRAT_PLAIN_IP
            : (strcmp(argv[1], "ip_pr") == 0)  ? STRAT_IP_PR
            : (strcmp(argv[1], "ip_em") == 0)  ? STRAT_IP_EM
                                               : (strategy_t)-1;
    if (strat == (strategy_t)-1) {
      fprintf(stderr, "Unknown strategy: %s\n", argv[1]);
      return 1;
    }
  }
  for (int i = 2; i < argc; ++i) {
    if (strncmp(argv[i], "--repeat=", 9) == 0) {
      int r = atoi(argv[i] + 9);
      if (r >= 1 && r <= 100000) repeat = r;
    } else if (!em_path_cli) {
      em_path_cli = argv[i];
    }
  }

  if (run_all) {
    bench_summary_t S_plain, S_pr, S_em;
    bench_all(repeat, em_path_cli, &S_plain, &S_pr, &S_em);
    print_table_header();
    print_table_row("plain_ip", &S_plain);
    print_table_row("ip_pr", &S_pr);
    print_table_row("ip_em", &S_em);
    return 0;
  }

  bench_summary_t S = bench_strategy(
      strat, repeat, (strat == STRAT_IP_EM) ? em_path_cli : NULL);
  print_table_header();
  const char* name = (strat == STRAT_PLAIN_IP) ? "plain_ip"
                     : (strat == STRAT_IP_PR)  ? "ip_pr"
                                               : "ip_em";
  print_table_row(name, &S);
  return 0;
}
