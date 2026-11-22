/**
 * @file ref_nn.c
 * @brief Reference int8 inference arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ref_nn.h"
#include <math.h>
#include <stddef.h>

static int32_t sat(int64_t v) {
  return v < INT32_MIN ? INT32_MIN : (v > INT32_MAX ? INT32_MAX : (int32_t)v);
}

/* gemmlowp SaturatingRoundingDoublingHighMul */
static int32_t srdhm(int32_t a, int32_t b) {
  if (a == INT32_MIN && b == INT32_MIN) {
    return INT32_MAX;
  }
  int64_t ab = (int64_t)a * b;
  int64_t nudge = ab >= 0 ? (1LL << 30) : (1 - (1LL << 30));
  return (int32_t)((ab + nudge) / (1LL << 31));
}

/* gemmlowp RoundingDivideByPOT */
static int32_t rdbpot(int32_t x, int32_t e) {
  int32_t mask = (int32_t)((1LL << e) - 1);
  int32_t rem = x & mask;
  int32_t thr = (mask >> 1) + (x < 0 ? 1 : 0);
  return (x >> e) + (rem > thr ? 1 : 0);
}

int32_t ref_requant(int32_t acc, int32_t mult, int32_t shift, RefRound mode) {
  if (mode == REF_ROUND_GEMMLOWP) {
    int32_t left = shift > 0 ? shift : 0;
    int32_t right = shift > 0 ? 0 : -shift;
    return rdbpot(srdhm(sat((int64_t)acc * (1LL << left)), mult), right);
  }
  int64_t ts = 31 - shift;
  int64_t p = (int64_t)acc * mult;
  int64_t h = 1LL << (ts - 1);
  if (mode == REF_ROUND_AWAY && p < 0) {
    return sat(-((-p + h) >> ts));
  }
  return sat((p + h) >> ts);
}

void ref_fully_connected(const int8_t *x, uint32_t n_in, const int8_t *w,
                         uint32_t n_out, const int32_t *b, int32_t zx,
                         const int32_t *mult, const int32_t *shift,
                         int per_channel, int32_t zy, int32_t lo, int32_t hi,
                         RefRound mode, int8_t *y) {
  for (uint32_t o = 0U; o < n_out; o++) {
    int64_t acc = b != NULL ? b[o] : 0;
    for (uint32_t i = 0U; i < n_in; i++) {
      acc += (int64_t)(x[i] - zx) * w[o * n_in + i];
    }
    uint32_t c = per_channel ? o : 0U;
    int64_t v =
        (int64_t)ref_requant((int32_t)acc, mult[c], shift[c], mode) + zy;
    y[o] = (int8_t)(v < lo ? lo : (v > hi ? hi : v));
  }
}

void ref_conv2d(const int8_t *x, uint32_t ih, uint32_t iw, uint32_t ic,
                const int8_t *w, uint32_t kh, uint32_t kw, uint32_t sh,
                uint32_t sw, uint32_t pt, uint32_t pl, uint32_t oh, uint32_t ow,
                uint32_t oc, const int32_t *b, int32_t zx, const int32_t *mult,
                const int32_t *shift, int per_channel, int32_t zy, int32_t lo,
                int32_t hi, RefRound mode, int8_t *y) {
  for (uint32_t oy = 0U; oy < oh; oy++) {
    for (uint32_t ox = 0U; ox < ow; ox++) {
      for (uint32_t o = 0U; o < oc; o++) {
        int64_t acc = b != NULL ? b[o] : 0;
        for (uint32_t ky = 0U; ky < kh; ky++) {
          for (uint32_t kx = 0U; kx < kw; kx++) {
            int64_t iy = (int64_t)(oy * sh + ky) - pt;
            int64_t ix = (int64_t)(ox * sw + kx) - pl;
            if (iy < 0 || ix < 0 || iy >= ih || ix >= iw) {
              continue;
            }
            for (uint32_t c = 0U; c < ic; c++) {
              int64_t xv = x[((uint64_t)iy * iw + (uint64_t)ix) * ic + c];
              acc += (xv - zx) * w[((o * kh + ky) * kw + kx) * ic + c];
            }
          }
        }
        uint32_t q = per_channel ? o : 0U;
        int64_t v =
            (int64_t)ref_requant((int32_t)acc, mult[q], shift[q], mode) + zy;
        y[(oy * ow + ox) * oc + o] = (int8_t)(v < lo ? lo : (v > hi ? hi : v));
      }
    }
  }
}

void ref_dwconv(const int8_t *x, uint32_t ih, uint32_t iw, uint32_t ic,
                const int8_t *w, uint32_t kh, uint32_t kw, uint32_t sh,
                uint32_t sw, uint32_t pt, uint32_t pl, uint32_t oh, uint32_t ow,
                uint32_t oc, const int32_t *b, int32_t zx, const int32_t *mult,
                const int32_t *shift, int per_channel, int32_t zy, int32_t lo,
                int32_t hi, RefRound mode, int8_t *y) {
  uint32_t m = oc / ic;
  for (uint32_t oy = 0U; oy < oh; oy++) {
    for (uint32_t ox = 0U; ox < ow; ox++) {
      for (uint32_t o = 0U; o < oc; o++) {
        int64_t acc = b != NULL ? b[o] : 0;
        for (uint32_t ky = 0U; ky < kh; ky++) {
          for (uint32_t kx = 0U; kx < kw; kx++) {
            int64_t iy = (int64_t)(oy * sh + ky) - pt;
            int64_t ix = (int64_t)(ox * sw + kx) - pl;
            if (iy < 0 || ix < 0 || iy >= ih || ix >= iw) {
              continue;
            }
            int64_t xv = x[((uint64_t)iy * iw + (uint64_t)ix) * ic + o / m];
            acc += (xv - zx) * w[(ky * kw + kx) * oc + o];
          }
        }
        uint32_t q = per_channel ? o : 0U;
        int64_t v =
            (int64_t)ref_requant((int32_t)acc, mult[q], shift[q], mode) + zy;
        y[(oy * ow + ox) * oc + o] = (int8_t)(v < lo ? lo : (v > hi ? hi : v));
      }
    }
  }
}

void ref_pool(const int8_t *x, uint32_t ih, uint32_t iw, uint32_t c,
              uint32_t kh, uint32_t kw, uint32_t sh, uint32_t sw, uint32_t pt,
              uint32_t pl, uint32_t oh, uint32_t ow, int avg, int32_t lo,
              int32_t hi, int8_t *y) {
  for (uint32_t oy = 0U; oy < oh; oy++) {
    for (uint32_t ox = 0U; ox < ow; ox++) {
      int64_t y0 = (int64_t)(oy * sh) - pt, x0 = (int64_t)(ox * sw) - pl;
      for (uint32_t ch = 0U; ch < c; ch++) {
        int64_t acc = avg ? 0 : -128, n = 0;
        for (int64_t iy = y0; iy < y0 + kh; iy++) {
          for (int64_t ix = x0; ix < x0 + kw; ix++) {
            if (iy < 0 || ix < 0 || iy >= ih || ix >= iw) {
              continue;
            }
            int64_t v = x[((uint64_t)iy * iw + (uint64_t)ix) * c + ch];
            acc = avg ? acc + v : (v > acc ? v : acc);
            n++;
          }
        }
        if (avg) {
          acc = acc > 0 ? (acc + n / 2) / n : (acc - n / 2) / n;
        }
        y[(oy * ow + ox) * c + ch] =
            (int8_t)(acc < lo ? lo : (acc > hi ? hi : acc));
      }
    }
  }
}

void ref_mean(const int8_t *x, uint32_t h, uint32_t w, uint32_t c, int32_t zx,
              double scale, int32_t zy, int32_t lo, int32_t hi, RefRound mode,
              int8_t *y) {
  int32_t mult, shift;
  ref_quantize_multiplier(scale, &mult, &shift);
  int64_t n = (int64_t)h * w;
  int s = 63 - __builtin_clzll((unsigned long long)n);
  s = s < 32 ? s : 32;
  s = s < 31 + shift ? s : 31 + shift;
  mult = (int32_t)(((int64_t)mult << s) / n);
  shift -= s;
  for (uint32_t ch = 0U; ch < c; ch++) {
    int64_t acc = 0;
    for (int64_t i = 0; i < n; i++) {
      acc += x[(uint64_t)i * c + ch];
    }
    acc -= zx * n;
    int64_t v = (int64_t)ref_requant((int32_t)acc, mult, shift, mode) + zy;
    y[ch] = (int8_t)(v < lo ? lo : (v > hi ? hi : v));
  }
}

void ref_quantize_multiplier(double scale, int32_t *mult, int32_t *shift) {
  if (scale == 0.0) {
    *mult = 0;
    *shift = 0;
    return;
  }
  int e;
  double q = frexp(scale, &e);
  int64_t m = (int64_t)round(q * (double)(1LL << 31));
  if (m == 1LL << 31) {
    m /= 2;
    e++;
  }
  if (e < -31) {
    e = 0;
    m = 0;
  }
  if (e > 30) {
    e = 30;
    m = (1LL << 31) - 1;
  }
  *mult = (int32_t)m;
  *shift = e;
}
