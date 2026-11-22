/**
 * @file test_fnn_convert.c
 * @brief Host tests for fnn_convert.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_convert.h"
#include "fnn_quant.h"
#include "ref_nn.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

#define MODE ((RefRound)FNN_ROUNDING)

static int8_t clamp8(int64_t v) {
  return (int8_t)(v < -128 ? -128 : (v > 127 ? 127 : v));
}

static float bits_f(uint32_t b) {
  float f;
  memcpy(&f, &b, 4U);
  return f;
}

static uint32_t f_bits(float f) {
  uint32_t b;
  memcpy(&b, &f, 4U);
  return b;
}

/* Rounds the float's exact value times the multiplier with ref_requant. */
static int8_t ref_f32(uint32_t b, int32_t mult, int32_t rsh, int32_t zp) {
  uint32_t e = (b >> 23) & 255U, f = b & 0x7FFFFFU;
  if (e == 255U && f != 0U) {
    return -128;
  }
  int64_t v;
  int32_t m = (int32_t)(e != 0U ? f | 0x800000U : f);
  m = (b >> 31) != 0U ? -m : m;
  int32_t sh = rsh + 150 - (int32_t)(e != 0U ? e : 1U);
  if (mult == 0 || m == 0 || sh > 30) {
    v = 0;
  } else if (sh < -31) {
    v = m < 0 ? -128 - zp : 127 - zp;
  } else {
    v = ref_requant(m, mult, -1 - sh, MODE);
  }
  return clamp8(v + zp);
}

static const double scales[] = {1.0 / 256.0, 0.0371, 0.5, 1.0, 3.7, 1e-6};
static const int32_t zps[] = {0, -128, 127, 9};

void test_fnn_quantize_q15(void) {
  static int16_t x[65536];
  static int8_t y[65536];
  for (uint32_t i = 0U; i < 65536U; i++) {
    x[i] = (int16_t)(i - 32768U);
  }
  unsigned bad = 0U;
  for (unsigned c = 0U; c < sizeof scales / sizeof scales[0]; c++) {
    int32_t mult;
    int8_t rsh;
    ASSERT_EQ(fnn_quant_from_scale(0x1p-15 / scales[c], &mult, &rsh), FNN_OK);
    for (unsigned z = 0U; z < sizeof zps / sizeof zps[0]; z++) {
      fnn_quantize_q15_s8(x, y, 65536U, mult, rsh, zps[z]);
      for (uint32_t i = 0U; i < 65536U; i++) {
        int64_t v = ref_requant(x[i], mult, -1 - rsh, MODE);
        bad += y[i] != clamp8(v + zps[z]);
      }
    }
  }
  ASSERT_EQ(bad, 0U);
}

void test_fnn_quantize_q31(void) {
  static int32_t x[4096];
  static int8_t y[4096];
  uint64_t r = 0x931U;
  for (uint32_t i = 0U; i < 4096U; i++) {
    x[i] = (int32_t)test_rng_u32(&r) >> (i % 32U);
  }
  x[0] = INT32_MIN;
  x[1] = INT32_MAX;
  x[2] = 0;
  unsigned bad = 0U;
  for (unsigned c = 0U; c < sizeof scales / sizeof scales[0]; c++) {
    int32_t mult;
    int8_t rsh;
    ASSERT_EQ(fnn_quant_from_scale(0x1p-31 / scales[c], &mult, &rsh), FNN_OK);
    for (unsigned z = 0U; z < sizeof zps / sizeof zps[0]; z++) {
      fnn_quantize_q31_s8(x, y, 4096U, mult, rsh, zps[z]);
      for (uint32_t i = 0U; i < 4096U; i++) {
        int64_t v = ref_requant(x[i], mult, -1 - rsh, MODE);
        bad += y[i] != clamp8(v + zps[z]);
      }
    }
  }
  ASSERT_EQ(bad, 0U);
}

void test_fnn_quantize_f32(void) {
  static float x[4096];
  static int8_t y[4096];
  static const uint32_t edge[] = {
      0x00000000U, 0x80000000U, 0x00000001U, 0x807FFFFFU, 0x00800000U,
      0x7F7FFFFFU, 0xFF7FFFFFU, 0x7F800000U, 0xFF800000U, 0x7FC00000U,
      0xFFC00000U, 0x7F800001U, 0x3F800000U, 0xBF800000U, 0x3F000000U};
  uint64_t r = 0xF32U;
  unsigned bad = 0U, near = 0U;
  for (unsigned c = 0U; c < sizeof scales / sizeof scales[0]; c++) {
    int32_t mult;
    int8_t rsh;
    ASSERT_EQ(fnn_quant_from_scale(1.0 / scales[c], &mult, &rsh), FNN_OK);
    for (unsigned rep = 0U; rep < 40U; rep++) {
      for (uint32_t i = 0U; i < 4096U; i++) {
        x[i] = rep % 2U
                   ? bits_f(test_rng_u32(&r))
                   : (float)(scales[c] *
                             ((double)(int32_t)test_rng_u32(&r) * 0x1p-23));
      }
      for (unsigned i = 0U; i < sizeof edge / sizeof edge[0]; i++) {
        x[i] = bits_f(edge[i]);
      }
      /* These are exact ties at the scales that are powers of two. */
      for (uint32_t i = 16U; i < 80U; i++) {
        x[i] = (float)((int32_t)i - 48) * 0.5f * (float)scales[c];
      }
      int32_t zp = zps[rep % 4U];
      fnn_quantize_f32_s8(x, y, 4096U, mult, rsh, zp);
      for (uint32_t i = 0U; i < 4096U; i++) {
        bad += y[i] != ref_f32(f_bits(x[i]), mult, rsh, zp);
        /* Within half a step of x / s, allowing for the 31-bit multiplier. */
        double w = (double)x[i] / scales[c] + (double)zp;
        if (isfinite(w) && w > -128.0 && w < 127.0 &&
            FNN_ROUNDING != FNN_ROUND_GEMMLOWP) {
          near += fabs((double)y[i] - w) > 0.5 + fabs(w) * 0x1p-29;
        }
      }
    }
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_EQ(near, 0U);

  int32_t mult;
  int8_t rsh;
  ASSERT_EQ(fnn_quant_from_scale(1.0, &mult, &rsh), FNN_OK);
  float in[6] = {INFINITY, -INFINITY, NAN, -NAN, 2.5f, -2.5f};
  int8_t out[6];
  fnn_quantize_f32_s8(in, out, 6U, mult, rsh, 3);
  ASSERT_EQ(out[0], 127);
  ASSERT_EQ(out[1], -128);
  ASSERT_EQ(out[2], -128);
  ASSERT_EQ(out[3], -128);
  ASSERT_EQ(out[4], 6);
  ASSERT_EQ(out[5], FNN_ROUNDING == FNN_ROUND_UP ? 1 : 0);
  /* a zero multiplier gives the zero point, even for infinity */
  fnn_quantize_f32_s8(in, out, 2U, 0, -1, 3);
  ASSERT_EQ(out[0], 3);
  ASSERT_EQ(out[1], 3);
}

void test_fnn_dequantize_q15(void) {
  static int8_t x[256];
  static int16_t y[256];
  for (uint32_t i = 0U; i < 256U; i++) {
    x[i] = (int8_t)(i - 128U);
  }
  /* 3 2^-16 and 2^-16 make ties at every odd input. */
  static const double ss[] = {1.0 / 256.0, 0.0371, 0.5,  1.0,     90.5,
                              181.0,       300.0,  1e-7, 0x3p-16, 0x1p-16};
  unsigned bad = 0U;
  for (unsigned c = 0U; c < sizeof ss / sizeof ss[0]; c++) {
    int32_t mult;
    int8_t rsh;
    ASSERT_EQ(fnn_quant_from_scale(0x1p15 * ss[c], &mult, &rsh), FNN_OK);
    for (unsigned z = 0U; z < sizeof zps / sizeof zps[0]; z++) {
      fnn_dequantize_s8_q15(x, y, 256U, mult, rsh, zps[z]);
      for (uint32_t i = 0U; i < 256U; i++) {
        int64_t v = ref_requant(x[i] - zps[z], mult, -1 - rsh, MODE);
        v = v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
        bad += y[i] != v;
      }
    }
  }
  ASSERT_EQ(bad, 0U);
}

/* Compares every input and zero point with the float multiply, bit for bit. */
static unsigned dequant_f32(float s) {
  static int8_t x[256];
  static float y[257];
  for (uint32_t i = 0U; i < 256U; i++) {
    x[i] = (int8_t)(i - 128U);
  }
  unsigned bad = 0U;
  for (int32_t zp = -128; zp < 128; zp += 17) {
    y[256] = 55.0f;
    fnn_dequantize_s8_f32(x, y, 256U, s, zp);
    bad += y[256] != 55.0f;
    for (uint32_t i = 0U; i < 256U; i++) {
      volatile float want = s * (float)(x[i] - zp);
      bad += f_bits(y[i]) != f_bits(want);
    }
  }
  return bad;
}

void test_fnn_dequantize_f32(void) {
  static const uint32_t edge[] = {
      0x3F800000U, 0x00800000U, 0x00000001U, 0x007FFFFFU, 0x7F7FFFFFU,
      0x7A7FFFFFU, 0x7A800000U, 0x7B000000U, 0x3FFFFFFFU, 0x3F800001U,
      0x3B800000U, 0x00FFFFFFU, 0x3DCCCCCDU};
  unsigned bad = 0U;
  for (unsigned i = 0U; i < sizeof edge / sizeof edge[0]; i++) {
    bad += dequant_f32(bits_f(edge[i]));
  }
  uint64_t r = 0xDE0U;
  for (unsigned i = 0U; i < 2000U; i++) {
    uint32_t b = test_rng_u32(&r) & 0x7FFFFFFFU;
    if (b >= 0x7F800000U) {
      continue;
    }
    bad += dequant_f32(bits_f(b));
  }
  ASSERT_EQ(bad, 0U);
}
