/**
 * @file cases_cobs.c
 * @brief Bench cases for fu_cobs.h, with the usual byte loops as candidates
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fu_cobs.h"

/* ---- candidates: one byte per step ---- */

BENCH_CALL static size_t enc_bytes(uint8_t *d, const uint8_t *s, size_t n) {
  size_t code_at = 0U;
  size_t o = 1U;
  uint8_t code = 1U;
  for (size_t i = 0U; i < n; i++) {
    if (s[i] != 0U) {
      d[o++] = s[i];
      code++;
    }
    if (s[i] == 0U || code == 0xFFU) {
      d[code_at] = code;
      code = 1U;
      if (s[i] == 0U || i + 1U < n) {
        code_at = o++;
      } else {
        return o;
      }
    }
  }
  d[code_at] = code;
  return o;
}

BENCH_CALL static bool dec_bytes(uint8_t *d, const uint8_t *s, size_t n,
                                 size_t *len) {
  size_t i = 0U;
  size_t o = 0U;
  while (i < n) {
    uint32_t code = s[i++];
    if (code == 0U || code - 1U > n - i) {
      return false;
    }
    for (uint32_t k = code - 1U; k != 0U; k--) {
      uint8_t b = s[i++];
      if (b == 0U) {
        return false;
      }
      d[o++] = b;
    }
    if (code != 0xFFU && i != n) {
      d[o++] = 0U;
    }
  }
  *len = o;
  return true;
}

/* ---- data: "random" has a 0 about 1 byte in 256, "sparse" 1 in 4 ---- */

#define FRAMES 8U
#define ENC_MAX FU_COBS_MAX(64U)
#define RAND_LEN (FRAMES * 64U + 256U)
/* in bench_scratch: data_rand, data_sparse, enc_rand, enc_sparse, out, rxbuf */
static uint8_t *const data_rand = bench_scratch;
static uint8_t *const data_sparse = bench_scratch + RAND_LEN;
static uint8_t (*const enc_rand)[ENC_MAX] =
    (uint8_t (*)[ENC_MAX])(bench_scratch + RAND_LEN + FRAMES * 64U);
static uint8_t (*const enc_sparse)[ENC_MAX] =
    (uint8_t (*)[ENC_MAX])(bench_scratch + RAND_LEN + FRAMES * (64U + ENC_MAX));
static uint8_t *const out =
    bench_scratch + RAND_LEN + FRAMES * (64U + 2U * ENC_MAX);
static uint8_t *const rxbuf =
    bench_scratch + RAND_LEN + FRAMES * (64U + 2U * ENC_MAX) + 300U;
_Static_assert(RAND_LEN + FRAMES * (64U + 2U * ENC_MAX) + 364U <=
                   BENCH_SCRATCH_SIZE,
               "cobs data");
static uint8_t enc_rand_len[FRAMES];
static uint8_t enc_sparse_len[FRAMES];
static FU_CobsRx rx;

static void data_init(void) {
  static const char id = 0;
  if (!bench_scratch_claim(&id)) {
    return;
  }
  uint32_t s = 0x9E3779B9U;
  for (size_t i = 0U; i < RAND_LEN; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    data_rand[i] = (uint8_t)(s >> 24);
    if (i < FRAMES * 64U) {
      data_sparse[i] = (s & 3U) == 0U ? 0U : (uint8_t)(1U + (s >> 8) % 255U);
    }
  }
  for (size_t f = 0U; f < FRAMES; f++) {
    enc_rand_len[f] =
        (uint8_t)fu_cobs_encode(enc_rand[f], &data_rand[64U * f], 64U);
    enc_sparse_len[f] =
        (uint8_t)fu_cobs_encode(enc_sparse[f], &data_sparse[64U * f], 64U);
  }
  fu_cobs_rx_init(&rx, rxbuf, 64U);
}

/* the frame to use */
static inline uint32_t in_frame(uint32_t u) {
  data_init();
  return u % FRAMES;
}

static size_t dec_len;

/* a whole frame through the receiver, a call per byte as from an ISR */
BENCH_CALL static uint32_t rx_frame(const uint8_t *e, size_t n) {
  size_t len = 0U;
  for (size_t i = 0U; i < n; i++) {
    (void)fu_cobs_rx_byte(&rx, e[i], &len);
  }
  return fu_cobs_rx_byte(&rx, 0U, &len) ? (uint32_t)len : 0xFFFFFFFFU;
}

#define OUT_HASH h = bench_hash(h, out, (unsigned)dec_len)
#define RX_HASH h = bench_hash(h, rxbuf, 64U)

BENCH_TEXT(c_enc_rand, uint32_t, in_frame,
           fu_cobs_encode(out, &data_rand[64U * x], 64U), out)
BENCH_TEXT(c_encb_rand, uint32_t, in_frame,
           enc_bytes(out, &data_rand[64U * x], 64U), out)
BENCH_TEXT(c_enc_sparse, uint32_t, in_frame,
           fu_cobs_encode(out, &data_sparse[64U * x], 64U), out)
BENCH_TEXT(c_encb_sparse, uint32_t, in_frame,
           enc_bytes(out, &data_sparse[64U * x], 64U), out)
BENCH_TEXT(c_enc_256, uint32_t, in_frame,
           fu_cobs_encode(out, &data_rand[64U * x], 256U), out)
BENCH_TEXT(c_encb_256, uint32_t, in_frame,
           enc_bytes(out, &data_rand[64U * x], 256U), out)
BENCH_UNARY_POST(c_dec_rand, uint32_t, in_frame, uint32_t,
                 fu_cobs_decode(out, enc_rand[x], enc_rand_len[x], &dec_len),
                 OUT_HASH)
BENCH_UNARY_POST(c_decb_rand, uint32_t, in_frame, uint32_t,
                 dec_bytes(out, enc_rand[x], enc_rand_len[x], &dec_len),
                 OUT_HASH)
BENCH_UNARY_POST(c_dec_sparse, uint32_t, in_frame, uint32_t,
                 fu_cobs_decode(out, enc_sparse[x], enc_sparse_len[x],
                                &dec_len),
                 OUT_HASH)
BENCH_UNARY_POST(c_decb_sparse, uint32_t, in_frame, uint32_t,
                 dec_bytes(out, enc_sparse[x], enc_sparse_len[x], &dec_len),
                 OUT_HASH)
BENCH_UNARY_POST(c_rx_rand, uint32_t, in_frame, uint32_t,
                 rx_frame(enc_rand[x], enc_rand_len[x]), RX_HASH)

/* encoders share a hash per data set, and so do decoders */
static const BenchCase cases[] = {
    {"fu_cobs_encode, 64 B random", "flexlib", c_enc_rand, true},
    {"cobs encode byte loop, 64 B random", "candidate", c_encb_rand, true},
    {"fu_cobs_encode, 64 B 1/4 zeros", "flexlib", c_enc_sparse, true},
    {"cobs encode byte loop, 64 B 1/4 zeros", "candidate", c_encb_sparse,
     true},
    {"fu_cobs_encode, 256 B random", "flexlib", c_enc_256, true},
    {"cobs encode byte loop, 256 B random", "candidate", c_encb_256, true},
    {"fu_cobs_decode, 64 B random", "flexlib", c_dec_rand, true},
    {"cobs decode byte loop, 64 B random", "candidate", c_decb_rand, true},
    {"fu_cobs_decode, 64 B 1/4 zeros", "flexlib", c_dec_sparse, true},
    {"cobs decode byte loop, 64 B 1/4 zeros", "candidate", c_decb_sparse,
     true},
    {"fu_cobs_rx_byte, 64 B frame + 0", "flexlib", c_rx_rand, true},
};

const BenchGroup bench_group_cobs = {cases, BENCH_COUNT(cases)};
