/**
 * @file fd_wide.h
 * @brief 128-bit helpers for exact block statistics (internal)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_WIDE_H
#define FD_WIDE_H

#include "fx_arch.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint64_t lo;
  uint64_t hi;
} FD_U128;

FX_INLINE FD_U128 fd_u128(uint64_t hi, uint64_t lo) {
  FD_U128 r = {lo, hi};
  return r;
}

FX_INLINE FD_U128 fd_u128_add(FD_U128 a, FD_U128 b) {
  FD_U128 r;
  r.lo = a.lo + b.lo;
  r.hi = a.hi + b.hi + (r.lo < a.lo ? 1U : 0U);
  return r;
}

FX_INLINE FD_U128 fd_u128_sub(FD_U128 a, FD_U128 b) {
  FD_U128 r;
  r.lo = a.lo - b.lo;
  r.hi = a.hi - b.hi - (a.lo < b.lo ? 1U : 0U);
  return r;
}

FX_INLINE bool fd_u128_ge(FD_U128 a, FD_U128 b) {
  return a.hi != b.hi ? a.hi > b.hi : a.lo >= b.lo;
}

/* a << k, k < 64 */
FX_INLINE FD_U128 fd_u128_shl(FD_U128 a, uint32_t k) {
  if (k == 0U) {
    return a;
  }
  return fd_u128((a.hi << k) | (a.lo >> (64U - k)), a.lo << k);
}

/* a >> k, k < 128 */
FX_INLINE FD_U128 fd_u128_shr(FD_U128 a, uint32_t k) {
  if (k == 0U) {
    return a;
  }
  if (k >= 64U) {
    return fd_u128(0U, a.hi >> (k - 64U));
  }
  return fd_u128(a.hi >> k, (a.lo >> k) | (a.hi << (64U - k)));
}

/* a b, full 128 bits */
FX_INLINE FD_U128 fd_u128_mul64(uint64_t a, uint64_t b) {
  uint32_t a0 = (uint32_t)a, a1 = (uint32_t)(a >> 32);
  uint32_t b0 = (uint32_t)b, b1 = (uint32_t)(b >> 32);
  uint64_t p00 = fx_umull(a0, b0);
  uint64_t p01 = fx_umull(a0, b1);
  uint64_t p10 = fx_umull(a1, b0);
  uint64_t p11 = fx_umull(a1, b1);
  uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
  return fd_u128(p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32),
                 (mid << 32) | (uint32_t)p00);
}

/* a m mod 2^128 */
FX_INLINE FD_U128 fd_u128_mul32(FD_U128 a, uint32_t m) {
  FD_U128 r = fd_u128_mul64(a.lo, m);
  r.hi += a.hi * m;
  return r;
}

/* (u1 2^32 + u0) / v with u1 < v, and the remainder (Hacker's Delight divlu) */
FX_INLINE uint32_t fd_divlu(uint32_t u1, uint32_t u0, uint32_t v,
                            uint32_t *rem) {
  const uint32_t s = fx_clz(v);
  v <<= s;
  const uint32_t vn1 = v >> 16, vn0 = v & 0xFFFFU;
  const uint32_t un32 = (u1 << s) | (uint32_t)((uint64_t)u0 >> (32U - s));
  const uint32_t un10 = u0 << s;
  const uint32_t un1 = un10 >> 16, un0 = un10 & 0xFFFFU;
  uint32_t q1 = un32 / vn1;
  uint32_t rhat = un32 - q1 * vn1;
  while (q1 > 0xFFFFU || q1 * vn0 > ((rhat << 16) | un1)) {
    q1--;
    rhat += vn1;
    if (rhat > 0xFFFFU) {
      break;
    }
  }
  const uint32_t un21 = (un32 << 16) + un1 - q1 * v;
  uint32_t q0 = un21 / vn1;
  rhat = un21 - q0 * vn1;
  while (q0 > 0xFFFFU || q0 * vn0 > ((rhat << 16) | un0)) {
    q0--;
    rhat += vn1;
    if (rhat > 0xFFFFU) {
      break;
    }
  }
  *rem = ((un21 << 16) + un0 - q0 * v) >> s;
  return (q1 << 16) + q0;
}

/* a = floor(a / d), returns the remainder; d != 0 */
FX_INLINE uint32_t fd_u128_divmod32(FD_U128 *a, uint32_t d) {
  if (d == 1U) {
    return 0U;
  }
  uint32_t w[4] = {(uint32_t)(a->hi >> 32), (uint32_t)a->hi,
                   (uint32_t)(a->lo >> 32), (uint32_t)a->lo};
  uint32_t r = 0U;
  for (unsigned i = 0U; i < 4U; i++) {
    if (r == 0U) {
      uint32_t q = w[i] / d;
      r = w[i] - q * d;
      w[i] = q;
    } else {
      w[i] = fd_divlu(r, w[i], d, &r);
    }
  }
  *a = fd_u128(((uint64_t)w[0] << 32) | w[1], ((uint64_t)w[2] << 32) | w[3]);
  return r;
}

/* q31 sums of products: 96 bits, hi:lo */
typedef struct {
  uint64_t lo;
  int32_t hi;
} Sum96;

FX_INLINE void sum96_add(Sum96 *s, int64_t p) {
  uint64_t t = s->lo + (uint64_t)p;
  s->hi += (t < s->lo ? 1 : 0) - (p < 0 ? 1 : 0);
  s->lo = t;
}

/* s += v 2^16 */
FX_INLINE void sum96_add_shl16(Sum96 *s, int64_t v) {
  uint64_t t = s->lo + ((uint64_t)v << 16);
  s->hi += (int32_t)(v >> 48) + (t < s->lo ? 1 : 0);
  s->lo = t;
}

/* exact sum of a b: b = bh 2^16 + bl, each int64 sum holding 2^16 terms */
FX_INLINE Sum96 sum96_dot(const int32_t *a, const int32_t *b, uint32_t n) {
  Sum96 s = {0U, 0};
  while (n > 0U) {
    uint32_t m = n < 65536U ? n : 65536U;
    int64_t hi = 0;
    int64_t lo = 0;
    FX_UNROLL(2)
    for (uint32_t i = 0U; i < m; i++) {
      hi = fx_smlal(hi, a[i], b[i] >> 16);
      lo = fx_smlal(lo, a[i], (int32_t)((uint32_t)b[i] & 0xFFFFU));
    }
    sum96_add(&s, lo);
    sum96_add_shl16(&s, hi);
    a += m;
    b += m;
    n -= m;
  }
  return s;
}

/* round(s / 2^14), saturated to int64 */
FX_INLINE int64_t sum96_q48(Sum96 s) {
  sum96_add(&s, 8192);
  if (s.hi >= 8192) {
    return INT64_MAX;
  }
  if (s.hi < -8192) {
    return INT64_MIN;
  }
  return (int64_t)(((uint64_t)(uint32_t)s.hi << 50) | (s.lo >> 14));
}

#endif /* FD_WIDE_H */
