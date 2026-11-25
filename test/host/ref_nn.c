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

void ref_add(const int8_t *x1, int32_t z1, double s1, const int8_t *x2,
             int32_t z2, double s2, double sy, int32_t zy, int32_t lo,
             int32_t hi, RefRound mode, uint32_t n, int8_t *y) {
  double big = s1 > s2 ? s1 : s2;
  int32_t m1, m2, mo, e1, e2, eo;
  if (mode == REF_ROUND_GEMMLOWP) {
    double twice = 2.0 * big;
    ref_quantize_multiplier(s1 / twice, &m1, &e1);
    ref_quantize_multiplier(s2 / twice, &m2, &e2);
    ref_quantize_multiplier(twice / (1048576.0 * sy), &mo, &eo);
  } else {
    ref_quantize_multiplier(big / sy, &mo, &eo);
    int64_t k = 31 - eo;
    int64_t ms = (int64_t)floor((s1 > s2 ? s2 : s1) / sy * ldexp(1.0, (int)k) + 0.5);
    m1 = s1 > s2 ? mo : (int32_t)ms;
    m2 = s1 > s2 ? (int32_t)ms : mo;
    e1 = (int32_t)k;
  }
  for (uint32_t i = 0U; i < n; i++) {
    int64_t d1 = x1[i] - z1, d2 = x2[i] - z2, v;
    if (mode == REF_ROUND_GEMMLOWP) {
      int32_t a = ref_requant((int32_t)(d1 * 1048576), m1, e1, mode);
      int32_t b = ref_requant((int32_t)(d2 * 1048576), m2, e2, mode);
      v = ref_requant(a + b, mo, eo, mode);
    } else {
      int64_t num = m1 * d1 + m2 * d2, h = 1LL << (e1 - 1);
      v = mode == REF_ROUND_AWAY && num < 0 ? -((-num + h) >> e1)
                                            : (num + h) >> e1;
    }
    v += zy;
    y[i] = (int8_t)(v < lo ? lo : (v > hi ? hi : v));
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

/* gemmlowp SaturatingRoundingMultiplyByPOT, exponent > 0 */
static int32_t sat_pot(int32_t x, int e) {
  int32_t thr = (int32_t)((1LL << (31 - e)) - 1);
  return x > thr ? INT32_MAX : (x < -thr ? INT32_MIN : (int32_t)(x * (1LL << e)));
}

/* gemmlowp exp_on_interval_between_negative_one_quarter_and_0_excl */
static int32_t exp_interval(int32_t a) {
  const int32_t constant_term = 1895147668, one_third = 715827883;
  int32_t x = a + (1 << 28);
  int32_t x2 = srdhm(x, x);
  int32_t x3 = srdhm(x2, x);
  int32_t x4 = srdhm(x2, x2);
  int32_t x4_over_4 = rdbpot(x4, 2);
  int32_t poly = rdbpot(srdhm(x4_over_4 + x3, one_third) + x2, 1);
  return constant_term + srdhm(constant_term, x + poly);
}

/* gemmlowp exp_on_negative_values, five integer bits in */
static int32_t exp_on_negative_values(int32_t a) {
  static const int32_t mult[7] = {1672461947, 1302514674, 790015084, 290630308,
                                  39332535,   720401,     242};
  int32_t quarter = 1 << 24;
  int32_t a_mod = (a & (quarter - 1)) - quarter;
  int32_t result = exp_interval(sat_pot(a_mod, 5));
  int32_t remainder = a_mod - a;
  for (int e = -2; e <= 4; e++) {
    if (remainder & (1 << (26 + e))) {
      result = srdhm(result, mult[e + 2]);
    }
  }
  return a == 0 ? INT32_MAX : result;
}

/* gemmlowp one_over_one_plus_x_for_x_in_0_1 */
static int32_t one_over_one_plus_x(int32_t a) {
  int64_t sum = (int64_t)a + INT32_MAX;
  int32_t half_denominator = (int32_t)((sum + (sum >= 0 ? 1 : -1)) / 2);
  int32_t x = 1515870810 + srdhm(half_denominator, -1010580540);
  for (int i = 0; i < 3; i++) {
    int32_t one_minus = (1 << 29) - srdhm(half_denominator, x);
    x = x + sat_pot(srdhm(x, one_minus), 2);
  }
  return sat_pot(x, 1);
}

/* TFLite's PreprocessSoftmaxScaling and CalculateInputRadius */
static void softmax_params(double k, int32_t *mult, int *left, int32_t *diff_min) {
  double real = fmin(k * (double)(1 << 26), 2147483647.0);
  double q = frexp(real, left);
  int64_t m = (int64_t)round(q * (double)(1LL << 31));
  if (m == 1LL << 31) {
    m /= 2;
    (*left)++;
  }
  *mult = (int32_t)m;
  *diff_min = -(int32_t)floor(31.0 * (double)(1LL << 26) / (double)(1LL << *left));
}

static int32_t softmax_exp(int32_t diff, int32_t mult, int left, int32_t diff_min) {
  if (diff < diff_min) {
    return 0;
  }
  return exp_on_negative_values(srdhm((int32_t)(diff * (1LL << left)), mult));
}

int32_t ref_softmax_exp(double k, uint32_t d) {
  int32_t mult, diff_min;
  int left;
  softmax_params(k, &mult, &left, &diff_min);
  return softmax_exp(-(int32_t)d, mult, left, diff_min);
}

void ref_softmax(const int8_t *x, uint32_t n, double k, int8_t *y) {
  int32_t m, diff_min;
  int left;
  softmax_params(k, &m, &left, &diff_min);
  int32_t mx = -128;
  for (uint32_t i = 0U; i < n; i++) {
    mx = x[i] > mx ? x[i] : mx;
  }
  int32_t sum = 0;
  for (uint32_t i = 0U; i < n; i++) {
    sum += rdbpot(softmax_exp(x[i] - mx, m, left, diff_min), 12);
  }
  int headroom = __builtin_clz((uint32_t)sum);
  int over = 12 - headroom;
  int32_t scale = one_over_one_plus_x((int32_t)(((uint32_t)sum << headroom) - (1U << 31)));
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v = -128;
    if (x[i] - mx >= diff_min) {
      int32_t p = srdhm(scale, softmax_exp(x[i] - mx, m, left, diff_min));
      int e = over + 31 - 8;
      v = (e <= 31 ? rdbpot(p, e) : 0) - 128;
    }
    y[i] = (int8_t)(v > 127 ? 127 : v);
  }
}
