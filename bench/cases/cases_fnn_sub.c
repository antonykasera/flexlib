/**
 * @file cases_fnn_sub.c
 * @brief Bench cases for convolutions with int4 and ternary weights
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fnn_conv.h"
#include "fx_arch.h"

#define XOFF 64U
#define xin ((int8_t *)(void *)bench_scratch)
#define yout ((int8_t *)(void *)(bench_scratch + 4160U))
#define sub ((int8_t *)(void *)(bench_scratch + 6464U))
_Static_assert(
    4160U + 2304U <= 6464U &&
        6464U + FNN_CONV_SUB_SCRATCH(3U, 3U, 8U) <= BENCH_SCRATCH_SIZE &&
        6464U + FNN_CONV_SUB_SCRATCH(1U, 1U, 64U) <= BENCH_SCRATCH_SIZE &&
        6464U + FNN_CONV_SUB_SCRATCH(1U, 5U, 16U) <= BENCH_SCRATCH_SIZE,
    "inputs, outputs and the scratch");

#define S4_61                                                                  \
  0xA5, 0x4D, 0xCA, 0x25, 0x30, 0xBB, 0x1D, 0x6D, 0x13, 0x2C, 0xDE, 0xD6,      \
      0x23, 0x7B, 0x2E, 0xD9, 0x1E, 0x3F, 0x72, 0x1F, 0xCB, 0x19, 0x71, 0x17,  \
      0x44, 0x94, 0xD6, 0x49, 0x3C, 0x9D, 0x5C, 0x34, 0x60, 0xBE, 0x31, 0x20,  \
      0x1E, 0x69, 0xFE, 0xDA, 0xA0, 0xEE, 0xB9, 0x99, 0x7F, 0x5C, 0x7C, 0x29,  \
      0x99, 0xFD, 0xAF, 0xE5, 0x93, 0x25, 0x3C, 0xD6, 0x54, 0xAF, 0x4D, 0xFA,  \
      0xD7
#define S2_61                                                                  \
  0x35, 0x7D, 0xD0, 0xDC, 0x3C, 0x55, 0x7F, 0x07, 0xFC, 0x33, 0xD1, 0x30,      \
      0x0F, 0x73, 0x05, 0x50, 0x5C, 0x17, 0x31, 0x1D, 0xC4, 0x1F, 0x07, 0x55,  \
      0x7F, 0x54, 0x0F, 0x70, 0xC4, 0xF1, 0x03, 0xC1, 0x0F, 0xF1, 0xCF, 0x3C,  \
      0xF5, 0x1F, 0x3C, 0xF0, 0x1D, 0x71, 0xF7, 0xC0, 0xDD, 0x1C, 0x57, 0x33,  \
      0xF4, 0x43, 0x3D, 0xF3, 0x5C, 0x05, 0x0C, 0xF0, 0x10, 0x77, 0xFF, 0x47,  \
      0x15
#define X4(v) v, v, v, v
static const uint8_t s4pool[61U * 34U] BENCH_TABLE FX_ALIGN(16) = {
    X4(X4(S4_61)), X4(X4(S4_61)), S4_61, S4_61};
static const uint8_t s2pool[61U * 17U] BENCH_TABLE FX_ALIGN(16) = {
    X4(X4(S2_61)), S2_61};
_Static_assert(sizeof s4pool >= FNN_S4_BYTES(64U, 64U) &&
                   sizeof s4pool >= FNN_S4_BYTES(80U, 32U) &&
                   sizeof s4pool >= FNN_S4_BYTES(72U, 16U) &&
                   sizeof s2pool >= FNN_S2_BYTES(64U, 64U) &&
                   sizeof s2pool >= FNN_S2_BYTES(80U, 32U),
               "the largest layer's weights");

static const int32_t bias[64] = {X4(X4(-1200)), X4(X4(900)), X4(X4(-300)),
                                 X4(X4(2100))};
static const int32_t mult = 1518500250;
static const int8_t rsh = 9;

#define LAYER(name, pool, ih, iw, ic, oh, ow, oc, k_h, k_w, p_t, p_l, pk)      \
  static const FNN_Conv name = {(const int8_t *)(pool),                        \
                                bias,                                          \
                                {ih, iw, ic},                                  \
                                {oh, ow, oc},                                  \
                                k_h,                                           \
                                k_w,                                           \
                                1U,                                            \
                                1U,                                            \
                                p_t,                                           \
                                p_l,                                           \
                                pk,                                            \
                                -3,                                            \
                                {&mult, &rsh, 1U},                             \
                                {5, -128, 127}}
LAYER(pw4, s4pool, 5U, 5U, 64U, 5U, 5U, 64U, 1U, 1U, 0U, 0U, FNN_PACK_S4);
LAYER(pw2, s2pool, 5U, 5U, 64U, 5U, 5U, 64U, 1U, 1U, 0U, 0U, FNN_PACK_S2);
LAYER(cv4, s4pool, 12U, 12U, 8U, 12U, 12U, 16U, 3U, 3U, 1U, 1U, FNN_PACK_S4);
LAYER(cv2, s2pool, 12U, 12U, 8U, 12U, 12U, 16U, 3U, 3U, 1U, 1U, FNN_PACK_S2);
LAYER(c14, s4pool, 1U, 68U, 16U, 1U, 64U, 32U, 1U, 5U, 0U, 0U, FNN_PACK_S4);
LAYER(c12, s2pool, 1U, 68U, 16U, 1U, 64U, 32U, 1U, 5U, 0U, 0U, FNN_PACK_S2);

static int scratch_id;

static uint32_t rnd(uint32_t *s) {
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

static inline uint32_t in_off(uint32_t u) {
  if (bench_scratch_claim(&scratch_id)) {
    uint32_t s = 0x6C078965U;
    for (unsigned i = 0U; i < 4096U + XOFF; i++) {
      xin[i] = (int8_t)(rnd(&s) >> 24);
    }
  }
  return u % XOFF;
}

static uint32_t conv(uint32_t off, const FNN_Conv *l) {
  fnn_conv_s8(l, xin + off, yout, sub);
  return 0U;
}

#define BLOCK(name, call, bytes)                                               \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, (uint32_t)(call),          \
                  h = bench_hash(h, yout, (bytes)), (void)0)
BLOCK(b_pw4, conv(x, &pw4), 1600U)
BLOCK(b_pw2, conv(x, &pw2), 1600U)
BLOCK(b_cv4, conv(x, &cv4), 2304U)
BLOCK(b_cv2, conv(x, &cv2), 2304U)
BLOCK(b_c14, conv(x, &c14), 2048U)
BLOCK(b_c12, conv(x, &c12), 2048U)

static const BenchCase cases[] = {
    {"fnn_conv_s8, 1 x 1, 64 -> 64, 5 x 5, int4", "flexlib", b_pw4, true},
    {"fnn_conv_s8, 1 x 1, 64 -> 64, 5 x 5, ternary", "flexlib", b_pw2, true},
    {"fnn_conv_s8, 3 x 3, 8 -> 16, 12 x 12, int4", "flexlib", b_cv4, true},
    {"fnn_conv_s8, 3 x 3, 8 -> 16, 12 x 12, ternary", "flexlib", b_cv2, true},
    {"fnn_conv_s8, 5 taps, 16 -> 32, 64, int4", "flexlib", b_c14, true},
    {"fnn_conv_s8, 5 taps, 16 -> 32, 64, ternary", "flexlib", b_c12, true},
};

const BenchGroup bench_group_fnn_sub = {cases, BENCH_COUNT(cases)};
