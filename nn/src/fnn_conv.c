/**
 * @file fnn_conv.c
 * @brief Convolution layers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_conv.h"
#include "fnn_copy.h"
#include "fnn_dense.h"
#include "fnn_internal.h"
#include "fx_arch.h"
#include <string.h>

#define NOINLINE __attribute__((noinline))

/* may_alias keeps word reads of packed weights and of the scratch defined under
 * LTO. */
typedef uint32_t __attribute__((may_alias)) Word;
typedef int32_t __attribute__((may_alias)) Field;

static int fits(uint32_t in, uint32_t out, uint32_t k, uint32_t s, uint32_t p) {
  if (in == 0U || out == 0U || k == 0U || s == 0U || p >= k) {
    return 0;
  }
  /* The last window starts inside the input, and ends less than k past it. */
  return (uint64_t)(out - 1U) * s < (uint64_t)in + p;
}

FNN_Status fnn_conv_check(const FNN_Conv *l) {
  if (!fits(l->in.h, l->out.h, l->kh, l->sh, l->pt) ||
      !fits(l->in.w, l->out.w, l->kw, l->sw, l->pl) || l->in.c == 0U ||
      l->out.c == 0U || (l->q.n != 1U && l->q.n != l->out.c)) {
    return FNN_ERR_SHAPE;
  }
  if (l->zx < -128 || l->zx > 127 || l->packed > FNN_PACK_S2) {
    return FNN_ERR_RANGE;
  }
  return FNN_OK;
}

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/* Copies the patch under the filter, one run per row, with the zero point
 * where it overhangs. */
FX_INLINE void patch(const FNN_Conv *l, const int8_t *x, int32_t iy, int32_t ix,
                  int8_t *p) {
  uint32_t c = l->in.c, run = l->kw * c;
  uint32_t kx0 = (uint32_t)clampi(-ix, 0, (int32_t)l->kw);
  uint32_t kx1 = (uint32_t)clampi((int32_t)l->in.w - ix, 0, (int32_t)l->kw);
  for (uint32_t ky = 0U; ky < l->kh; ky++, p += run) {
    int32_t y = iy + (int32_t)ky;
    if (y < 0 || y >= (int32_t)l->in.h) {
      fnn_fill(p, l->zx, run);
      continue;
    }
    const int8_t *row = x + ((uint32_t)y * l->in.w + (uint32_t)(ix + (int32_t)kx0)) * c;
    fnn_fill(p, l->zx, kx0 * c);
    fnn_copy(p + kx0 * c, row, (kx1 - kx0) * c);
    fnn_fill(p + kx1 * c, l->zx, (l->kw - kx1) * c);
  }
}

#define FIELD(v, at, bits)                                                     \
  ((int32_t)((v) << (32U - (at) - (bits))) >> (32U - (bits)))

/* Splits each field sum into the two pixels' sums. */
FX_INLINE void split(const int32_t *s, uint32_t *a) {
  for (uint32_t j = 0U; j < 8U; j++) {
    int32_t lo = (int32_t)((uint32_t)s[j] << 16) >> 16;
    a[2U * j] += (uint32_t)lo;
    a[2U * j + 1U] += (uint32_t)((s[j] - lo) >> 16);
  }
}

/* Eight channels' int4 weights for one input a word, against its field; 36
 * products of at most 7 x 128 keep each half inside int16. */
static NOINLINE void sum_s4(const Word *w, const Field *x, uint32_t n,
                            uint32_t *a) {
  for (uint32_t i = 0U; i < n;) {
    uint32_t end = n - i > 36U ? i + 36U : n;
    int32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0, s7 = 0;
    for (; i < end; i++) {
      uint32_t v = w[i];
      int32_t f = x[i];
      s0 += FIELD(v, 0U, 4U) * f;
      s1 += FIELD(v, 4U, 4U) * f;
      s2 += FIELD(v, 8U, 4U) * f;
      s3 += FIELD(v, 12U, 4U) * f;
      s4 += FIELD(v, 16U, 4U) * f;
      s5 += FIELD(v, 20U, 4U) * f;
      s6 += FIELD(v, 24U, 4U) * f;
      s7 += FIELD(v, 28U, 4U) * f;
    }
    const int32_t s[8] = {s0, s1, s2, s3, s4, s5, s6, s7};
    split(s, a);
  }
}

/* Eight channels' ternary weights for two inputs a word; 254 products of at
 * most 128. */
static NOINLINE void sum_s2(const Word *w, const Field *x, uint32_t n,
                            uint32_t *a) {
  for (uint32_t i = 0U; i < n;) {
    uint32_t end = n - i > 254U ? i + 254U : n;
    int32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0, s7 = 0;
    for (; i < end; i += 2U) {
      uint32_t v = w[i / 2U];
      int32_t f = x[i], g = x[i + 1U];
      s0 += FIELD(v, 0U, 2U) * f + FIELD(v, 16U, 2U) * g;
      s1 += FIELD(v, 2U, 2U) * f + FIELD(v, 18U, 2U) * g;
      s2 += FIELD(v, 4U, 2U) * f + FIELD(v, 20U, 2U) * g;
      s3 += FIELD(v, 6U, 2U) * f + FIELD(v, 22U, 2U) * g;
      s4 += FIELD(v, 8U, 2U) * f + FIELD(v, 24U, 2U) * g;
      s5 += FIELD(v, 10U, 2U) * f + FIELD(v, 26U, 2U) * g;
      s6 += FIELD(v, 12U, 2U) * f + FIELD(v, 28U, 2U) * g;
      s7 += FIELD(v, 14U, 2U) * f + FIELD(v, 30U, 2U) * g;
    }
    const int32_t s[8] = {s0, s1, s2, s3, s4, s5, s6, s7};
    split(s, a);
  }
}

/* Output pixel (oy, ox)'s patch: its run in x, or a copy in p. */
static const int8_t *pixel(const FNN_Conv *l, const int8_t *x, uint32_t oy,
                           uint32_t ox, int8_t *p) {
  int32_t iy = (int32_t)(oy * l->sh) - (int32_t)l->pt;
  int32_t ix = (int32_t)(ox * l->sw) - (int32_t)l->pl;
  if (iy >= 0 && iy + (int32_t)l->kh <= (int32_t)l->in.h && ix >= 0 &&
      ix + (int32_t)l->kw <= (int32_t)l->in.w &&
      (l->kh == 1U || l->kw == l->in.w)) {
    return x + ((uint32_t)iy * l->in.w + (uint32_t)ix) * l->in.c;
  }
  patch(l, x, iy, ix, p);
  return p;
}

static void put(const FNN_Conv *l, uint32_t o, uint32_t sum, int8_t *y) {
  uint32_t c = l->q.n == 1U ? 0U : o;
  int32_t v = fnn_requant_as((int32_t)sum, l->q.mult[c], l->q.rsh[c], FNN_RULE);
  *y = fnn_clamp_s8(v, l->o.zp, l->o.lo, l->o.hi);
}

/* Two output pixels a pass, their patches' values in the 16-bit fields of
 * x0 + 65536 x1, which one multiply per weight adds to both sums. */
static NOINLINE void conv_sub(const FNN_Conv *l, const int8_t *x, int8_t *y,
                              int8_t *scratch) {
  const uint32_t n = l->kh * l->kw * l->in.c, m = l->out.c;
  const uint32_t words = l->packed == FNN_PACK_S4 ? n : (n + 1U) / 2U;
  int8_t *pa = scratch, *pb = scratch + n;
  Field *f = (Field *)(void *)(scratch + ((2U * n + 3U) & ~3U));
  FX_ASSERT(((uintptr_t)scratch & 3U) == 0U && ((uintptr_t)l->w & 3U) == 0U);
  f[n] = 0;
  uint32_t oy = 0U, ox = 0U;
  for (uint32_t left = l->out.h * l->out.w; left != 0U;) {
    const int8_t *p0 = pixel(l, x, oy, ox, pa), *p1 = p0;
    uint32_t two = left >= 2U;
    if (++ox == l->out.w) {
      ox = 0U;
      oy++;
    }
    if (two) {
      p1 = pixel(l, x, oy, ox, pb);
      if (++ox == l->out.w) {
        ox = 0U;
        oy++;
      }
    }
    for (uint32_t i = 0U; i < n; i++) {
      f[i] = p0[i] + 65536 * p1[i];
    }
    const Word *w = (const Word *)(const void *)l->w;
    for (uint32_t o = 0U; o < m; o += 8U, w += words) {
      uint32_t a[16], k = m - o < 8U ? m - o : 8U;
      for (uint32_t j = 0U; j < 8U; j++) {
        a[2U * j] = a[2U * j + 1U] = j < k && l->b != NULL ? (uint32_t)l->b[o + j] : 0U;
      }
      (l->packed == FNN_PACK_S4 ? sum_s4 : sum_s2)(w, f, n, a);
      for (uint32_t j = 0U; j < k; j++) {
        put(l, o + j, a[2U * j], y + o + j);
        if (two) {
          put(l, o + j, a[2U * j + 1U], y + m + o + j);
        }
      }
    }
    y += two ? 2U * m : m;
    left -= two ? 2U : 1U;
  }
}

/* Groups of eight rows: per input (int4) or pair of inputs (ternary) one word,
 * row j of the group in field j. */
static FNN_Status pack(const int8_t *w, uint32_t n, uint32_t rows,
                       uint32_t bits, int8_t *packed) {
  const uint32_t per = bits == 4U ? 1U : 2U, words = (n + per - 1U) / per;
  const int32_t lim = bits == 4U ? 7 : 1;
  for (uint32_t g = 0U; g < (rows + 7U) / 8U; g++) {
    for (uint32_t k = 0U; k < words; k++) {
      uint32_t v = 0U;
      for (uint32_t t = 0U; t < per && k * per + t < n; t++) {
        for (uint32_t j = 0U; j < 8U && 8U * g + j < rows; j++) {
          int32_t q = w[(8U * g + j) * n + k * per + t];
          if (q < -lim || q > lim) {
            return FNN_ERR_RANGE;
          }
          v |= ((uint32_t)q & ((1U << bits) - 1U)) << (bits * (8U * t + j));
        }
      }
      memcpy(packed + 4U * (g * words + k), &v, 4U);
    }
  }
  return FNN_OK;
}

FNN_Status fnn_conv_pack_s4(const int8_t *w, uint32_t n, uint32_t rows,
                            int8_t *packed) {
  return pack(w, n, rows, 4U, packed);
}

FNN_Status fnn_conv_pack_s2(const int8_t *w, uint32_t n, uint32_t rows,
                            int8_t *packed) {
  return pack(w, n, rows, 2U, packed);
}

static NOINLINE void conv_s8(const FNN_Conv *l, const int8_t *x, int8_t *y,
                             int8_t *scratch) {
  uint32_t c = l->in.c, m = l->out.c;
  /* Each output pixel is this dense layer over its patch. */
  const FNN_Dense d = {l->w, l->b, l->kh * l->kw * c, m, l->packed, l->q, l->o};
  for (uint32_t oy = 0U; oy < l->out.h; oy++) {
    int32_t iy = (int32_t)(oy * l->sh) - (int32_t)l->pt;
    int rows_in = iy >= 0 && iy + (int32_t)l->kh <= (int32_t)l->in.h;
    for (uint32_t ox = 0U; ox < l->out.w; ox++) {
      int32_t ix = (int32_t)(ox * l->sw) - (int32_t)l->pl;
      const int8_t *p;
      /* A window inside the input whose rows are one run needs no copy. */
      if (rows_in && ix >= 0 && ix + (int32_t)l->kw <= (int32_t)l->in.w &&
          (l->kh == 1U || l->kw == l->in.w)) {
        p = x + ((uint32_t)iy * l->in.w + (uint32_t)ix) * c;
      } else {
        patch(l, x, iy, ix, scratch);
        p = scratch;
      }
      FNN_DENSE_CONV(&d, p, y);
      y += m;
    }
  }
}

void fnn_conv_s8(const FNN_Conv *l, const int8_t *x, int8_t *y,
                 int8_t *scratch) {
  if (l->packed >= FNN_PACK_S4) {
    conv_sub(l, x, y, scratch);
  } else {
    conv_s8(l, x, y, scratch);
  }
}
