/**
 * @file fnn_pool.c
 * @brief Pooling layers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_pool.h"
#include "fnn_quant.h"
#include "fx_arch.h"
#include <string.h>

#define TOP 0x80808080U

static int fits(uint32_t in, uint32_t out, uint32_t k, uint32_t s, uint32_t p) {
  if (in == 0U || out == 0U || k == 0U || s == 0U || p >= k) {
    return 0;
  }
  return (uint64_t)(out - 1U) * s < (uint64_t)in + p;
}

FNN_Status fnn_pool_check(const FNN_Pool *p) {
  if (!fits(p->in.h, p->out.h, p->kh, p->sh, p->pt) ||
      !fits(p->in.w, p->out.w, p->kw, p->sw, p->pl) || p->in.c == 0U ||
      p->out.c != p->in.c) {
    return FNN_ERR_SHAPE;
  }
  if (p->lo < -128 || p->hi > 127 || p->lo > p->hi) {
    return FNN_ERR_RANGE;
  }
  return FNN_OK;
}

static uint32_t ld(const int8_t *p) {
  uint32_t v;
  memcpy(&v, p, 4U);
  return v;
}

/* 0xFF in each byte where a's int8 is at least b's, else 0. */
static uint32_t ge4(uint32_t a, uint32_t b) {
  uint32_t t = (a | TOP) - (b & ~TOP);
  uint32_t g = ((b & ~a) | (t & ~(a ^ b))) & TOP;
  return (g << 1) - (g >> 7);
}

static uint32_t max4(uint32_t a, uint32_t b) {
  return b ^ ((a ^ b) & ge4(a, b));
}

static uint32_t min4(uint32_t a, uint32_t b) {
  return a ^ ((a ^ b) & ge4(a, b));
}

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/* Returns the window's first tap inside the input, and its size there. */
static const int8_t *window(const FNN_Pool *p, const int8_t *x, uint32_t oy,
                            uint32_t ox, uint32_t *nky, uint32_t *nkx) {
  int32_t iy = (int32_t)(oy * p->sh) - (int32_t)p->pt;
  int32_t ix = (int32_t)(ox * p->sw) - (int32_t)p->pl;
  int32_t ky0 = clampi(-iy, 0, (int32_t)p->kh);
  int32_t kx0 = clampi(-ix, 0, (int32_t)p->kw);
  int32_t ky1 = clampi((int32_t)p->in.h - iy, 0, (int32_t)p->kh);
  int32_t kx1 = clampi((int32_t)p->in.w - ix, 0, (int32_t)p->kw);
  *nky = (uint32_t)(ky1 - ky0);
  *nkx = (uint32_t)(kx1 - kx0);
  return x + ((uint32_t)(iy + ky0) * p->in.w + (uint32_t)(ix + kx0)) * p->in.c;
}

void fnn_maxpool_s8(const FNN_Pool *p, const int8_t *x, int8_t *y) {
  uint32_t c = p->in.c, row = p->in.w * c;
  int32_t lo = p->lo, hi = p->hi;
  int clamp = lo > -128 || hi < 127;
  uint32_t lo4 = (uint32_t)(uint8_t)lo * 0x01010101U;
  uint32_t hi4 = (uint32_t)(uint8_t)hi * 0x01010101U;
  for (uint32_t oy = 0U; oy < p->out.h; oy++) {
    for (uint32_t ox = 0U; ox < p->out.w; ox++, y += c) {
      uint32_t nky, nkx, j = 0U;
      const int8_t *w = window(p, x, oy, ox, &nky, &nkx);
      /* Four channels at a time, compared bytewise in one word. */
      for (; j + 4U <= c; j += 4U) {
        const int8_t *r = w + j;
        uint32_t m = TOP;
        for (uint32_t ky = 0U; ky < nky; ky++, r += row) {
          for (uint32_t kx = 0U; kx < nkx; kx++) {
            m = max4(m, ld(r + kx * c));
          }
        }
        if (clamp) {
          m = min4(max4(m, lo4), hi4);
        }
        memcpy(y + j, &m, 4U);
      }
      for (; j < c; j++) {
        int32_t m = -128;
        for (uint32_t ky = 0U; ky < nky; ky++) {
          for (uint32_t kx = 0U; kx < nkx; kx++) {
            int32_t v = w[ky * row + kx * c + j];
            m = v > m ? v : m;
          }
        }
        y[j] = (int8_t)clampi(m, lo, hi);
      }
    }
  }
}

FX_INLINE void sum4(const int8_t *r, uint32_t nky, uint32_t nkx, uint32_t c,
                    uint32_t row, int32_t *s) {
  int32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
  for (uint32_t ky = 0U; ky < nky; ky++, r += row) {
    const int8_t *q = r;
    for (uint32_t kx = nkx; kx > 0U; kx--, q += c) {
      s0 += q[0];
      s1 += q[1];
      s2 += q[2];
      s3 += q[3];
    }
  }
  s[0] = s0;
  s[1] = s1;
  s[2] = s2;
  s[3] = s3;
}

static int32_t avg(int32_t s, int32_t n) {
  return (s > 0 ? s + n / 2 : s - n / 2) / n;
}

void fnn_avgpool_s8(const FNN_Pool *p, const int8_t *x, int8_t *y) {
  uint32_t c = p->in.c, row = p->in.w * c;
  int32_t lo = p->lo, hi = p->hi;
  for (uint32_t oy = 0U; oy < p->out.h; oy++) {
    for (uint32_t ox = 0U; ox < p->out.w; ox++, y += c) {
      uint32_t nky, nkx, j = 0U;
      const int8_t *w = window(p, x, oy, ox, &nky, &nkx);
      uint32_t n = nky * nkx;
      for (; j + 4U <= c; j += 4U) {
        int32_t s[4];
        sum4(w + j, nky, nkx, c, row, s);
        for (uint32_t i = 0U; i < 4U; i++) {
          y[j + i] = (int8_t)clampi(avg(s[i], (int32_t)n), lo, hi);
        }
      }
      for (; j < c; j++) {
        int32_t s = 0;
        for (uint32_t ky = 0U; ky < nky; ky++) {
          for (uint32_t kx = 0U; kx < nkx; kx++) {
            s += w[ky * row + kx * c + j];
          }
        }
        y[j] = (int8_t)clampi(avg(s, (int32_t)n), lo, hi);
      }
    }
  }
}

FNN_Status fnn_mean_quant(double scale, uint32_t n, int32_t *mult,
                          int8_t *rsh) {
  int32_t m;
  int8_t r;
  if (n == 0U) {
    return FNN_ERR_SHAPE;
  }
  FNN_Status st = fnn_quant_from_scale(scale, &m, &r);
  if (st != FNN_OK) {
    return st;
  }
  /* TFLite divides the multiplier by n, truncating, after raising it by up
   * to log2(n) bits. */
  int32_t sh = 31 - (int32_t)fx_clz(n);
  sh = sh < 30 - r ? sh : 30 - r;
  *mult = (int32_t)(((int64_t)m << sh) / (int64_t)n);
  *rsh = (int8_t)(r + sh);
  return FNN_OK;
}

FNN_Status fnn_mean_check(const FNN_Mean *m) {
  if (m->in.h == 0U || m->in.w == 0U || m->in.c == 0U) {
    return FNN_ERR_SHAPE;
  }
  if (m->zx < -128 || m->zx > 127) {
    return FNN_ERR_RANGE;
  }
  return FNN_OK;
}

void fnn_mean_s8(const FNN_Mean *m, const int8_t *x, int8_t *y) {
  uint32_t c = m->in.c, n = m->in.h * m->in.w, j = 0U;
  int32_t bias = m->zx * (int32_t)n;
  for (; j + 4U <= c; j += 4U) {
    int32_t s[4];
    sum4(x + j, 1U, n, c, 0U, s);
    for (uint32_t i = 0U; i < 4U; i++) {
      int32_t v = fnn_requant(s[i] - bias, m->mult, m->rsh);
      y[j + i] = fnn_clamp_s8(v, m->o.zp, m->o.lo, m->o.hi);
    }
  }
  for (; j < c; j++) {
    int32_t s = 0;
    for (uint32_t i = 0U; i < n; i++) {
      s += x[i * c + j] - m->zx;
    }
    y[j] = fnn_clamp_s8(fnn_requant(s, m->mult, m->rsh), m->o.zp, m->o.lo,
                        m->o.hi);
  }
}
