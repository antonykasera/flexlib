/**
 * @file fnn_dense.c
 * @brief Fully connected layers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_dense.h"
#include "fnn_internal.h"
#include "fnn_quant.h"
#include <stddef.h>
#include <string.h>

#define NOINLINE __attribute__((noinline))

typedef uint32_t __attribute__((may_alias)) Word;

typedef void Rows(const int8_t *x, const int8_t *w, uint32_t n, uint32_t *sum);

/* Eight packed rows: one double word of weights per input. */
static NOINLINE void dense_r8(const int8_t *x, const int8_t *w, uint32_t n,
                              uint32_t *sum) {
  const Word *q = (const Word *)(const void *)w;
  uint32_t a[8];
  for (uint32_t j = 0U; j < 8U; j++) {
    a[j] = sum[j];
  }
#pragma GCC unroll 2
  for (uint32_t i = 0U; i < n; i++) {
    int32_t xi = x[i];
    uint32_t v = q[2U * i], u = q[2U * i + 1U];
    a[0] += (uint32_t)(xi * (int8_t)v);
    a[1] += (uint32_t)(xi * (int8_t)(v >> 8));
    a[2] += (uint32_t)(xi * (int8_t)(v >> 16));
    a[3] += (uint32_t)(xi * ((int32_t)v >> 24));
    a[4] += (uint32_t)(xi * (int8_t)u);
    a[5] += (uint32_t)(xi * (int8_t)(u >> 8));
    a[6] += (uint32_t)(xi * (int8_t)(u >> 16));
    a[7] += (uint32_t)(xi * ((int32_t)u >> 24));
  }
  for (uint32_t j = 0U; j < 8U; j++) {
    sum[j] = a[j];
  }
}

/* Four packed rows: one word of weights per input. */
static NOINLINE void dense_r4p(const int8_t *x, const int8_t *w, uint32_t n,
                               uint32_t *sum) {
  const Word *q = (const Word *)(const void *)w;
  uint32_t a0 = sum[0], a1 = sum[1], a2 = sum[2], a3 = sum[3];
  for (uint32_t i = 0U; i < n; i++) {
    int32_t xi = x[i];
    uint32_t v = q[i];
    a0 += (uint32_t)(xi * (int8_t)v);
    a1 += (uint32_t)(xi * (int8_t)(v >> 8));
    a2 += (uint32_t)(xi * (int8_t)(v >> 16));
    a3 += (uint32_t)(xi * ((int32_t)v >> 24));
  }
  sum[0] = a0;
  sum[1] = a1;
  sum[2] = a2;
  sum[3] = a3;
}

/* Four rows in TFLite's order, each input loaded once for all four. */
static NOINLINE void dense_r4(const int8_t *x, const int8_t *w, uint32_t n,
                              uint32_t *sum) {
  const int8_t *w0 = w, *w1 = w + n, *w2 = w1 + n, *w3 = w2 + n;
  uint32_t a0 = sum[0], a1 = sum[1], a2 = sum[2], a3 = sum[3];
  for (uint32_t i = 0U; i < n; i++) {
    int32_t xi = x[i];
    a0 += (uint32_t)(xi * w0[i]);
    a1 += (uint32_t)(xi * w1[i]);
    a2 += (uint32_t)(xi * w2[i]);
    a3 += (uint32_t)(xi * w3[i]);
  }
  sum[0] = a0;
  sum[1] = a1;
  sum[2] = a2;
  sum[3] = a3;
}

static void dense_r1(const int8_t *x, const int8_t *w, uint32_t n,
                     uint32_t *sum) {
  uint32_t a = *sum;
  for (uint32_t i = 0U; i < n; i++) {
    a += (uint32_t)(x[i] * w[i]);
  }
  *sum = a;
}

FX_INLINE void dense(const FNN_Dense *l, const int8_t *x, int8_t *y, int mode) {
  const int8_t *w = l->w;
  const int32_t *b = l->b;
  const int32_t *mult = l->q.mult;
  const int8_t *rsh = l->q.rsh;
  uint32_t n = l->n_in, m = l->n_out, step = l->q.n == 1U ? 0U : 1U;
  uint32_t packed = l->packed;
  int32_t zp = l->o.zp, lo = l->o.lo, hi = l->o.hi;
  FX_ASSERT(l->q.n == 1U || l->q.n == m);
  FX_ASSERT(!packed || ((uintptr_t)w & 3U) == 0U);
  for (uint32_t o = 0U; o < m;) {
    uint32_t rows = m - o >= 8U && packed ? 8U : (m - o >= 4U ? 4U : 1U);
    Rows *k = rows == 8U   ? dense_r8
              : rows == 4U ? (packed ? dense_r4p : dense_r4)
                           : dense_r1;
    uint32_t sum[8];
    for (uint32_t j = 0U; j < rows; j++) {
      sum[j] = b != NULL ? (uint32_t)b[o + j] : 0U;
    }
    k(x, w + o * n, n, sum);
    for (uint32_t j = 0U; j < rows; j++) {
      uint32_t c = (o + j) * step;
      int32_t v = fnn_requant_as((int32_t)sum[j], mult[c], rsh[c], mode);
      y[o + j] = fnn_clamp_s8(v, zp, lo, hi);
    }
    o += rows;
  }
}

void fnn_dense_s8(const FNN_Dense *l, const int8_t *x, int8_t *y) {
  dense(l, x, y, FNN_RULE_FC);
}

#if FNN_RULE_FC != FNN_RULE
void fnn_dense_conv_s8(const FNN_Dense *l, const int8_t *x, int8_t *y) {
  dense(l, x, y, FNN_RULE);
}
#endif

static void interleave(const int8_t *w, uint32_t n, uint32_t rows, int8_t *p) {
  for (uint32_t i = 0U; i < n; i++) {
    for (uint32_t j = 0U; j < rows; j++) {
      p[i * rows + j] = w[j * n + i];
    }
  }
}

void fnn_dense_pack_s8(const int8_t *w, uint32_t n_in, uint32_t n_out,
                       int8_t *packed) {
  uint32_t o = 0U;
  for (; n_out - o >= 8U; o += 8U) {
    interleave(w + o * n_in, n_in, 8U, packed + o * n_in);
  }
  if (n_out - o >= 4U) {
    interleave(w + o * n_in, n_in, 4U, packed + o * n_in);
    o += 4U;
  }
  memcpy(packed + o * n_in, w + o * n_in, (n_out - o) * n_in);
}

void fnn_fold_bias_s8(const int8_t *w, uint32_t n_in, uint32_t n_out,
                      const int32_t *b, int32_t zx, int32_t *b_out) {
  for (uint32_t o = 0U; o < n_out; o++) {
    int32_t s = 0;
    for (uint32_t i = 0U; i < n_in; i++) {
      s += w[o * n_in + i];
    }
    uint32_t v = (b != NULL ? (uint32_t)b[o] : 0U) - (uint32_t)zx * (uint32_t)s;
    b_out[o] = (int32_t)v;
  }
}
