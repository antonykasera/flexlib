/**
 * @file fm_div.c
 * @brief Division in Q15, Q31 and Q16.16, and 1/x in Q16.16
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_div.h"

FX_INLINE uint32_t mag(int32_t x) {
  return x < 0 ? 0U - (uint32_t)x : (uint32_t)x;
}

/* floor(m 2^16 / d) and remainder, d >= 2^31, m < d 2^16: divlu (Hacker's
 * Delight), two UDIV digits; a digit >= 2^16 always fails the product test */
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

/* q = n / d rounded, q <= 2^31; past INT32_MAX only -2^31 is in range */
FX_INLINE int32_t apply_sign(uint32_t q, int32_t sign) {
  if (sign < 0) {
    return (int32_t)(0U - q);
  }
  return q > (uint32_t)INT32_MAX ? INT32_MAX : (int32_t)q;
}

/* |a| >= |b|: at least 1 in magnitude, or a division by 0 */
FX_INLINE int32_t saturate(uint32_t ua, int32_t sign, int32_t max) {
  if (ua == 0U) {
    return 0;
  }
  return sign < 0 ? -max - 1 : max;
}

fm_q15 fm_div_q15(fm_q15 a, fm_q15 b) {
  uint32_t ua = mag(a);
  uint32_t ub = mag(b);
  int32_t sign = a ^ b;
  if (ua >= ub) {
    return (fm_q15)saturate(ua, sign, INT16_MAX);
  }
  uint32_t q = ((ua << 15) + (ub >> 1)) / ub;
  return (fm_q15)FX_SSAT(sign < 0 ? -(int32_t)q : (int32_t)q, 16);
}

fm_q31 fm_div_q31(fm_q31 a, fm_q31 b) {
  uint32_t ua = mag(a);
  uint32_t ub = mag(b);
  int32_t sign = a ^ b;
  if (ua >= ub) {
    return saturate(ua, sign, INT32_MAX);
  }
  if (ub == 0x80000000U) {
    return apply_sign(ua, sign);
  }
  uint32_t s = fx_clz(ub); /* >= 1 */
  uint32_t d = ub << s;
  uint32_t r;
  uint32_t q = udiv_norm((uint64_t)(ua << (s - 1U)) << 16, d, &r);
  return apply_sign(q + (r >= d - r ? 1U : 0U), sign);
}

fm_q16 fm_div_q16(fm_q16 a, fm_q16 b) {
  uint32_t ua = mag(a);
  uint32_t ub = mag(b);
  int32_t sign = a ^ b;
  if ((ua >> 15) >= ub) { /* |a / b| >= 2^15 */
    return saturate(ua, sign, INT32_MAX);
  }
  uint32_t q;
  if (ua < 0x10000U) { /* |a| < 1: a 32-bit divide */
    uint32_t n = ua << 16;
    q = n / ub;
    uint32_t r = n - q * ub;
    q += r >= ub - r ? 1U : 0U;
  } else {
    uint32_t s = fx_clz(ub);
    uint32_t d = ub << s;
    uint32_t r;
    q = udiv_norm((uint64_t)ua << s, d, &r);
    q += r >= d - r ? 1U : 0U;
  }
  return apply_sign(q, sign);
}

/* 2^32 = q d + r + 1 from one UDIV of 2^32 - 1 */
fm_q16 fm_recip_q16(fm_q16 x) {
  uint32_t d = mag(x);
  if (d < 2U) { /* 0 and +-2^-16 */
    return x < 0 ? INT32_MIN : INT32_MAX;
  }
  uint32_t q = 0xFFFFFFFFU / d;
  uint32_t r = 0xFFFFFFFFU - q * d + 1U; /* in [1, d] */
  return apply_sign(q + (r >= d - r ? 1U : 0U), x);
}
