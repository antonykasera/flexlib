/**
 * @file fm_internal.h
 * @brief Helpers shared by flexmath sources (not part of the API)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_INTERNAL_H
#define FM_INTERNAL_H

#include "fm_tables.h"
#include "fu_bits.h"
#include "fx_arch.h"
#include <stdint.h>

FX_INLINE uint32_t mag(int32_t x) {
  return x < 0 ? 0U - (uint32_t)x : (uint32_t)x;
}

/* floor(sqrt(x)) for x in [2^30, 2^32): table seed, one Newton step */
FX_INLINE uint32_t sqrt_norm32(uint32_t x) {
  uint32_t s = fm_sqrt_seed[(x >> 25) - 32U];
  s = (s + x / s) >> 1;
  s -= s >> 16; /* 65536 -> 65535, so s * s can't wrap */
  return s * s > x ? s - 1U : s;
}

/* floor(sqrt(x)) for x in [2^62, 2^64) */
FX_INLINE uint32_t sqrt_norm64(uint64_t x) {
  uint32_t h = sqrt_norm32((uint32_t)(x >> 32));
  uint64_t d = x - ((uint64_t)(h * h) << 32);
  uint64_t s = ((uint64_t)h << 16) + (uint32_t)(d >> 17) / h;
  uint32_t r = s > 0xFFFFFFFFU ? 0xFFFFFFFFU : (uint32_t)s;
  return (uint64_t)r * r > x ? r - 1U : r;
}

#define LN2_Q32 2977044472U

/* 2^(k / 64 + r) in Q38, r = r38 2^-38 < 2^-6 */
FX_INLINE uint64_t exp2_q38(uint32_t k, uint32_t r38) {
  uint32_t a = fx_umull_hi(r38, LN2_Q32);
  uint32_t a2 = fx_umull_hi(a, a); /* a^2, Q44 */
  uint32_t a3 = (((a2 >> 16) * (a >> 16)) >> 12) * 10923U >> 16;
  uint32_t a4 = (a3 * (a >> 16)) >> 24;
  uint32_t q = a + (a2 >> 7) + a3 + a4;
  uint32_t t = fm_exp2_tab[k];
  return (1ULL << 38) + ((uint64_t)t << 6) + fm_exp2_lo[k] + q +
         fx_umull_hi(t, q);
}

/* floor(m 2^16 / d) and remainder, d >= 2^31, m < d 2^16 (divlu) */
FX_INLINE uint32_t udiv_norm(uint64_t m, uint32_t d, uint32_t *rem) {
  uint32_t nh = (uint32_t)(m >> 16);
  uint32_t n1 = (uint32_t)m & 0xFFFFU;
  uint32_t d1 = d >> 16;
  uint32_t d0 = d & 0xFFFFU;
  uint32_t q1 = nh / d1;
  uint32_t r = nh - q1 * d1;
  while (q1 * d0 > ((r << 16) | n1)) {
    q1--;
    r += d1;
    if (r > 0xFFFFU) {
      break;
    }
  }
  uint32_t u = ((nh << 16) | n1) - q1 * d;
  uint32_t q0 = u / d1;
  r = u - q0 * d1;
  while (q0 * d0 > (r << 16)) {
    q0--;
    r += d1;
    if (r > 0xFFFFU) {
      break;
    }
  }
  *rem = (u << 16) - q0 * d;
  return (q1 << 16) | q0;
}

/* q31 sums of products: s is the sum mod 2^64, h about the sum / 2^34 */
typedef struct {
  uint64_t s;
  int32_t h;
} SumQ31;

FX_INLINE void sum_mac(SumQ31 *a, int32_t x, int32_t y) {
  int64_t p = fx_smull(x, y);
  a->s += (uint64_t)p;
  a->h += (int32_t)(p >> 32) >> 2;
}

FX_INLINE void sum_msub(SumQ31 *a, int32_t x, int32_t y) {
  int64_t p = fx_smull(x, y);
  a->s -= (uint64_t)p;
  a->h -= (int32_t)(p >> 32) >> 2;
}

/* round(sum / 2^31), saturated; h decides saturation, s the value */
FX_INLINE int32_t sum_q31(SumQ31 a) {
  if ((uint32_t)a.h + 0x18000000U >= 0x30000000U) {
    return a.h < 0 ? INT32_MIN : INT32_MAX;
  }
  int64_t r = ((int64_t)a.s + (1LL << 30)) >> 31;
  return r > INT32_MAX ? INT32_MAX : (r < INT32_MIN ? INT32_MIN : (int32_t)r);
}

/* float bits as m 2^(e - 150 - sh); zero has e = -1024, inf/NaN set *bad */
FX_INLINE int32_t unp_f32(uint32_t b, uint32_t sh, int32_t *e, uint32_t *bad) {
  uint32_t x = (b >> 23) & 255U;
  uint32_t m;
  if (__builtin_expect(x - 1U < 254U, 1)) {
    m = ((b << 8) | 0x80000000U) >> (8U - sh);
    *e = (int32_t)x;
  } else { /* zero, subnormal, inf, NaN */
    m = (b & 0x7FFFFFU) << sh;
    *e = m != 0U ? 1 : -1024;
    *bad |= x == 255U ? 1U : 0U;
  }
  return (int32_t)b < 0 ? -(int32_t)m : (int32_t)m;
}

/* float bits of s 2^(emax - 313), |s| < 2^63, ties away from zero */
FX_INLINE uint32_t f32_of_sum(int64_t s, int32_t emax) {
  uint32_t sign = (uint32_t)((uint64_t)s >> 63) << 31;
  uint64_t a = s < 0 ? 0U - (uint64_t)s : (uint64_t)s;
  uint32_t hi = (uint32_t)(a >> 32);
  uint32_t lz;
  uint32_t n;
  if (__builtin_expect(hi != 0U, 1)) { /* lz >= 1 */
    lz = fx_clz(hi);
    n = (hi << lz) | ((uint32_t)a >> (32U - lz));
  } else { /* cancellation */
    if ((uint32_t)a == 0U) {
      return 0U;
    }
    lz = fx_clz((uint32_t)a);
    n = (uint32_t)a << lz;
    lz += 32U;
  }
  int32_t e = emax - 123 - (int32_t)lz;
  if (e >= 1) {
    if (e > 254) {
      return sign | 0x7F800000U;
    }
    return sign | (((uint32_t)(e - 1) << 23) + (((n >> 7) + 1U) >> 1));
  }
  uint32_t r = (uint32_t)(8 - e); /* subnormal */
  return sign | (r < 32U ? ((n >> r) + 1U) >> 1 : 0U);
}

/* p >> sh, sh <= 63: 32-bit steps for sh < 32 */
FX_INLINE int64_t asr64(int64_t p, uint32_t sh) {
  if (__builtin_expect(sh < 32U, 1)) {
    uint32_t hi = (uint32_t)((uint64_t)p >> 32);
    uint32_t lo = ((uint32_t)p >> sh) | ((hi << 1) << (31U - sh));
    return (int64_t)(((uint64_t)(uint32_t)((int32_t)hi >> sh) << 32) | lo);
  }
  return p >> sh;
}

/* exact sum of n products, aligned to the largest exponent: s 2^(emax - 313) */
FX_INLINE int64_t sum_f32_s(const int32_t *ma, const int32_t *ea,
                            const int32_t *mb, const int32_t *eb, uint32_t bs,
                            uint32_t n, int32_t *emax) {
  int32_t em = ea[0] + eb[0];
#pragma GCC unroll 4
  for (uint32_t i = 1U; i < n; i++) {
    int32_t ep = ea[i] + eb[i * bs];
    em = ep > em ? ep : em;
  }
  int64_t s = 0;
#pragma GCC unroll 4
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t sh = (uint32_t)(em - (ea[i] + eb[i * bs]));
    s += asr64(fx_smull(ma[i], mb[i * bs]), sh < 63U ? sh : 63U);
  }
  *emax = em;
  return s;
}

FX_INLINE uint32_t sum_f32(const int32_t *ma, const int32_t *ea,
                           const int32_t *mb, const int32_t *eb, uint32_t bs,
                           uint32_t n) {
  int32_t emax;
  int64_t s = sum_f32_s(ma, ea, mb, eb, bs, n, &emax);
  return f32_of_sum(s, emax);
}

#endif /* FM_INTERNAL_H */
