/**
 * @file fm_float.c
 * @brief Float functions on integer internals, for cores without an FPU
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_float.h"
#include "fm_internal.h"
#include <string.h>

#define F32_QNAN 0x7FC00000U

FX_INLINE uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

FX_INLINE float u2f(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

/* sqrt of m 2^(e - 150) as float bits, m8 = m << 8 with m in [2^23, 2^24).
 * floor(sqrt(hi 2^32)) holds 8 bits below the result's LSB, so the nearest is
 * (floor + 128) >> 8 exactly; hi 2^32 has a zero low word, so the remainder
 * step is 32-bit. Its s is the floor or one above, which only changes the
 * result when s & 0xFF == 0x80. */
FX_INLINE uint32_t sqrt_bits(uint32_t m8, int32_t e) {
  uint32_t hi = m8 >> ((uint32_t)e & 1U);
  uint32_t h = sqrt_norm32(hi);
  uint32_t s = (h << 16) + ((hi - h * h) << 15) / h;
  if ((s & 0xFFU) == 0x80U && fx_umull(s, s) > (uint64_t)hi << 32) {
    s--;
  }
  return ((uint32_t)((e + 125) >> 1) << 23) + ((s + 128U) >> 8);
}

/* +-0, subnormals, negatives, inf and NaN */
FX_COLD static float sqrtf_special(uint32_t b) {
  if ((b << 1) == 0U || b == 0x7F800000U) {
    return u2f(b);
  }
  if ((b & 0x7FFFFFFFU) > 0x7F800000U) {
    return u2f(b | 0x00400000U);
  }
  if ((int32_t)b < 0) {
    return u2f(F32_QNAN);
  }
  uint32_t lz = fx_clz(b); /* subnormal: b 2^-149, lz >= 9 */
  return u2f(sqrt_bits(b << lz, 9 - (int32_t)lz));
}

float fm_sqrtf(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return sqrtf_special(b);
  }
  return u2f(sqrt_bits((b << 8) | 0x80000000U, (int32_t)(b >> 23)));
}

/* round(2^47 / m), m in [2^23, 2^24). y < 2^40 / m by at most ~5, so e = 2^40 -
 * y m fits a word; q = floor(2^48 / m) or one below. 2^48 = 0 mod 2^32 makes
 * each remainder one MUL. */
FX_INLINE uint32_t recip_mant(uint32_t m) {
  uint32_t y = 0xFFFFFFFFU / ((m >> 8) + 1U);
  uint32_t e = 0U - y * m;
  uint32_t q = (y << 8) + (uint32_t)(fx_umull(e, y) >> 32);
  if (0U - q * m >= m) {
    q++;
  }
  return (q + 1U) >> 1;
}

/* +-0, subnormals in and out, inf and NaN */
FX_COLD static float recipf_special(uint32_t b) {
  uint32_t sign = b & 0x80000000U;
  uint32_t a = b & 0x7FFFFFFFU;
  if (a >= 0x7F800000U) {
    return u2f(a == 0x7F800000U ? sign : b | 0x00400000U);
  }
  if (a == 0U) {
    return u2f(sign | 0x7F800000U);
  }
  uint32_t m = (a & 0x7FFFFFU) | 0x800000U;
  int32_t e = (int32_t)(a >> 23);
  if (e >= 253) { /* subnormal result, round(2^(299 - e) / m) */
    uint32_t rem;
    uint32_t q = udiv_norm(1ULL << (292 - e), m << 8, &rem);
    return u2f(sign | ((q + 1U) >> 1));
  }
  uint32_t lz = fx_clz(a) - 8U; /* subnormal input */
  e = 1 - (int32_t)lz;
  uint32_t r = ((uint32_t)(252 - e) << 23) + recip_mant(a << lz);
  return u2f(sign | (r >= 0x7F800000U ? 0x7F800000U : r));
}

float fm_recipf(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b << 1;
  if (a - 0x01000000U >= 0xFC000000U) { /* |x| < 2^-126 or >= 2^126 */
    return recipf_special(b);
  }
  uint32_t e = a >> 24;
  return u2f((b & 0x80000000U) + ((252U - e) << 23) +
             recip_mant((b & 0x7FFFFFU) | 0x800000U));
}

/* (2r + 1)^2 hi - 2^80, exact: the true difference is ~2^58 */
FX_INLINE int64_t rsqrt_side(uint32_t hi, uint32_t r) {
  uint64_t a = fx_umull(2U * r + 1U, 2U * r + 1U);
  return (int64_t)(fx_umull((uint32_t)a, hi) +
                   ((uint64_t)((uint32_t)(a >> 32) * hi) << 32));
}

/* 1/sqrt of m 2^(e - 150) as float bits, m8 = m << 8. R = round(2^47 /
 * sqrt(hi 2^16)) = round(y / 128) with y = 2^46 / sqrt(hi) from a seed and two
 * Newton steps. Over every hi, y - exact is in (-26.1, 4.9): R can be one low
 * only when y & 127 is 38..63, one high only when it is 64..68. */
FX_INLINE uint32_t rsqrt_bits(uint32_t m8, int32_t e) {
  uint32_t hi = m8 >> ((uint32_t)e & 1U);
  uint32_t y = (uint32_t)fm_rsqrt_seed[(hi >> 25) - 32U] << 16;
  for (int i = 0; i < 2; i++) {
    uint32_t y2 = (uint32_t)(fx_umull(y, y) >> 32);  /* 1/(4t), Q30 */
    uint32_t p = (uint32_t)(fx_umull(hi, y2) >> 32); /* t y^2, Q28 */
    y = (uint32_t)(fx_umull(y, (3U << 28) - p) >> 29);
  }
  uint32_t R = (y + 64U) >> 7;
  uint32_t g = (y - 38U) & 127U;
  if (g < 31U) {
    if (g < 26U) {
      R += rsqrt_side(hi, R) < 0 ? 1U : 0U;
    } else {
      R -= rsqrt_side(hi, R - 1U) > 0 ? 1U : 0U;
    }
  }
  return ((uint32_t)((378 - e) >> 1) << 23) + R;
}

/* +-0, subnormals, negatives, inf and NaN */
FX_COLD static float rsqrtf_special(uint32_t b) {
  if ((b << 1) == 0U) {
    return u2f(b | 0x7F800000U);
  }
  if ((b & 0x7FFFFFFFU) > 0x7F800000U) {
    return u2f(b | 0x00400000U);
  }
  if ((int32_t)b < 0) {
    return u2f(F32_QNAN);
  }
  if (b == 0x7F800000U) {
    return u2f(0U);
  }
  uint32_t lz = fx_clz(b);
  return u2f(rsqrt_bits(b << lz, 9 - (int32_t)lz));
}

float fm_rsqrtf(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return rsqrtf_special(b);
  }
  return u2f(rsqrt_bits((b << 8) | 0x80000000U, (int32_t)(b >> 23)));
}
