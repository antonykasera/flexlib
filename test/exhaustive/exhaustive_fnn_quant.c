/**
 * @file exhaustive_fnn_quant.c
 * @brief fnn_requant on every int32 sum, for multipliers and shifts at the ends
 * of their ranges, in the build's rounding mode
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_quant.h"
#include <stdio.h>

static int64_t sat(int64_t v) {
  return v < INT32_MIN ? INT32_MIN : (v > INT32_MAX ? INT32_MAX : v);
}

/* the exact product rounded the way the build's mode determines */
static int64_t ref(int64_t acc, int64_t mult, int32_t rsh) {
#if FNN_ROUNDING == FNN_ROUND_GEMMLOWP
  int64_t shift = -1 - rsh;
  int64_t x = sat(acc * (1LL << (shift > 0 ? shift : 0)));
  int64_t v =
      (x * mult + (1LL << 30)) >> 31; /* gemmlowp's nudge rounds ties up */
  int64_t e = shift > 0 ? 0 : -shift;
  if (e == 0) {
    return v;
  }
  int64_t h = 1LL << (e - 1);
  return v < 0 ? -((-v + h) >> e) : (v + h) >> e;
#else
  int64_t ts = 32 + rsh;
  int64_t p = acc * mult, h = 1LL << (ts - 1);
#if FNN_ROUNDING == FNN_ROUND_AWAY
  if (p < 0) {
    return sat(-((-p + h) >> ts));
  }
#endif
  return sat((p + h) >> ts);
#endif
}

int main(void) {
  static const int32_t mults[] = {1 << 30, 1518500250, INT32_MAX};
  static const int32_t rshs[] = {-31, -1, 0, 7, 30};
  unsigned long long bad = 0U;
  for (unsigned mi = 0U; mi < 3U; mi++) {
    for (unsigned si = 0U; si < 5U; si++) {
      int32_t m = mults[mi], s = rshs[si];
#pragma omp parallel for reduction(+ : bad) schedule(static)
      for (long long i = 0; i < (1LL << 32); i++) {
        int32_t a = (int32_t)(uint32_t)i;
        bad += fnn_requant(a, m, s) != ref(a, m, s);
      }
    }
  }
  printf("fnn_requant, rounding mode %d: 15 x 2^32 sums, %llu wrong\n",
         FNN_ROUNDING, bad);
  return bad == 0U ? 0 : 1;
}
