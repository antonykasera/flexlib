/**
 * @file fm_sum.h
 * @brief Exact sums of products for flexmath and flexdsp (internal, not API)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_SUM_H
#define FM_SUM_H

#include "fx_arch.h"
#include <stdint.h>

/* q31 sums of products, exact up to 7 terms: s = sum mod 2^64, h ~ sum/2^34 */
typedef struct {
  uint64_t s;
  int32_t h;
} FM_SumQ31;

FX_INLINE void fm_sum_mac(FM_SumQ31 *a, int32_t x, int32_t y) {
  int64_t p = fx_smull(x, y);
  a->s += (uint64_t)p;
  a->h += (int32_t)(p >> 32) >> 2;
}

FX_INLINE void fm_sum_msub(FM_SumQ31 *a, int32_t x, int32_t y) {
  int64_t p = fx_smull(x, y);
  a->s -= (uint64_t)p;
  a->h -= (int32_t)(p >> 32) >> 2;
}

/* round(sum / 2^31), saturated; h decides saturation, s the value */
FX_INLINE int32_t fm_sum_q31(FM_SumQ31 a) {
  if ((uint32_t)a.h + 0x18000000U >= 0x30000000U) {
    return a.h < 0 ? INT32_MIN : INT32_MAX;
  }
  int64_t r = ((int64_t)a.s + (1LL << 30)) >> 31;
  return r > INT32_MAX ? INT32_MAX : (r < INT32_MIN ? INT32_MIN : (int32_t)r);
}

/* float bits as m 2^(e - 150 - sh); zero has e = -1024, inf/NaN set *bad */
FX_INLINE int32_t fm_unp_f32(uint32_t b, uint32_t sh, int32_t *e,
                             uint32_t *bad) {
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
FX_INLINE uint32_t fm_f32_of_sum(int64_t s, int32_t emax) {
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
FX_INLINE int64_t fm_asr64(int64_t p, uint32_t sh) {
  if (__builtin_expect(sh < 32U, 1)) {
    uint32_t hi = (uint32_t)((uint64_t)p >> 32);
    uint32_t lo = ((uint32_t)p >> sh) | ((hi << 1) << (31U - sh));
    return (int64_t)(((uint64_t)(uint32_t)((int32_t)hi >> sh) << 32) | lo);
  }
  return p >> sh;
}

/* s 2^(emax - 313), within 2^-32 sum|p|; needs n 2^(48 + sa + sb) <= 2^63 */
FX_INLINE int64_t fm_sum_f32_s(const int32_t *ma, const int32_t *ea,
                               const int32_t *mb, const int32_t *eb,
                               uint32_t bs, uint32_t n, int32_t *emax) {
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
    s += fm_asr64(fx_smull(ma[i], mb[i * bs]), sh < 63U ? sh : 63U);
  }
  *emax = em;
  return s;
}

FX_INLINE uint32_t fm_sum_f32(const int32_t *ma, const int32_t *ea,
                              const int32_t *mb, const int32_t *eb, uint32_t bs,
                              uint32_t n) {
  int32_t emax;
  int64_t s = fm_sum_f32_s(ma, ea, mb, eb, bs, n, &emax);
  return fm_f32_of_sum(s, emax);
}

#endif /* FM_SUM_H */
