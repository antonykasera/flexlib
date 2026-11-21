/**
 * @file fd_stats_f32.c
 * @brief Float sums of products: exact in 64 bits, one rounding
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_bfp.h"
#include "fd_vec.h"
#include "fm_float.h"
#include "fm_sum.h"
#include <string.h>

/* s 2^(e - 309): 5 + 4 bits of headroom */
typedef struct {
  int64_t s;
  int32_t e;
  uint32_t bad;
} Acc;

FX_INLINE int32_t unp(const float *x, uint32_t sh, int32_t *e, uint32_t *bad) {
  uint32_t b;
  memcpy(&b, x, sizeof b);
  return fm_unp_f32(b, sh, e, bad);
}

FX_INLINE void add(Acc *a, int64_t t, int32_t e) {
  if (FX_UNLIKELY(e > a->e)) {
    uint32_t d = (uint32_t)(e - a->e);
    a->s = fm_asr64(a->s, d < 63U ? d : 63U);
    a->e = e;
  } else {
    uint32_t d = (uint32_t)(a->e - e);
    t = fm_asr64(t, d < 63U ? d : 63U);
  }
  a->s += t;
}

/* terms below 2^57: 16 of them keep |s| < 2^63 */
FX_INLINE void norm(Acc *a) {
  uint64_t m = a->s < 0 ? 0U - (uint64_t)a->s : (uint64_t)a->s;
  if (m >= (1ULL << 62)) {
    a->s >>= 1;
    a->e++;
  }
}

static Acc f32_sum(const float *a, const float *b, uint32_t n, int kind) {
  Acc acc = {0, -4096, 0U};
  for (uint32_t i = 0U; i < n; i++) {
    int32_t ea, eb;
    int32_t ma = unp(&a[i], kind == 1 ? 4U : 5U, &ea, &acc.bad);
    if (kind == 0) {
      int32_t mb = unp(&b[i], 4U, &eb, &acc.bad);
      add(&acc, fx_smull(ma, mb), ea + eb);
    } else if (kind == 1) {
      add(&acc, fx_smull(ma, ma), 2 * ea + 1);
    } else {
      add(&acc, (int64_t)ma * (1LL << 27), ea + 127);
    }
    if ((i & 15U) == 15U) {
      norm(&acc);
    }
  }
  norm(&acc);
  return acc;
}

static float nan_f32(void) {
  const uint32_t b = 0x7FC00000U;
  float f;
  memcpy(&f, &b, sizeof f);
  return f;
}

static float round_acc(Acc a) {
  return a.bad != 0U ? nan_f32() : fd_bfp_f32_of_s64(a.s, a.e - 309);
}

/* the exact sum over n, one rounding: n >= 2 leaves q < 2^62 */
static float div_acc(Acc a, uint32_t n) {
  if (a.bad != 0U) {
    return nan_f32();
  }
  if (n == 1U || a.s == 0) {
    return fd_bfp_f32_of_s64(a.s, a.e - 309);
  }
  uint64_t m = a.s < 0 ? 0U - (uint64_t)a.s : (uint64_t)a.s;
  uint32_t hi = (uint32_t)(m >> 32);
  uint32_t lz = hi != 0U ? fx_clz(hi) : 32U + fx_clz((uint32_t)m);
  m <<= lz - 1U;
  uint64_t q = m / n;
  int64_t v = (int64_t)((q << 1) | (m % n != 0U ? 1U : 0U));
  return fd_bfp_f32_of_s64(a.s < 0 ? -v : v, a.e - 309 - (int32_t)lz);
}

float fd_dot_f32(const float *a, const float *b, uint32_t n) {
  return round_acc(f32_sum(a, b, n, 0));
}

float fd_power_f32(const float *x, uint32_t n) {
  return round_acc(f32_sum(x, x, n, 1));
}

float fd_sum_f32(const float *x, uint32_t n) {
  return round_acc(f32_sum(x, x, n, 2));
}

float fd_mean_f32(const float *x, uint32_t n) {
  FX_ASSERT(n >= 1U);
  return div_acc(f32_sum(x, x, n, 2), n);
}

float fd_rms_f32(const float *x, uint32_t n) {
  FX_ASSERT(n >= 1U);
  return fm_sqrtf(div_acc(f32_sum(x, x, n, 1), n));
}
