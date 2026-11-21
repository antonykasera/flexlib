/**
 * @file ref_nn.c
 * @brief Reference int8 inference arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ref_nn.h"
#include <math.h>

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
