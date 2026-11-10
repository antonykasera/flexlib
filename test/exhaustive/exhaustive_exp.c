/**
 * @file exhaustive_exp.c
 * @brief The fm_exp.h logs on every positive input
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_exp.h"
#include <math.h>
#include <stdio.h>

/* double references: 2^-48 or better here, far below 2^-16 */
static double sat(double v) { return v > 2147483647.0 ? 2147483647.0 : v; }

static uint64_t mix(uint64_t z) {
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

int main(void) {
  double wl2 = 0.0, wln = 0.0, wl10 = 0.0, wdb = 0.0, we2 = 0.0, we = 0.0;
  double wp = 0.0;

#pragma omp parallel for reduction(max : wl2, wln, wl10, wdb, we2, we, wp)     \
    schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    int32_t x = (int32_t)(uint32_t)i;
    double v = x / 65536.0;
    if (x > 0) {
      wl2 = fmax(wl2, fabs(fm_log2_q16(x) - log2(v) * 65536.0));
      wln = fmax(wln, fabs(fm_ln_q16(x) - log(v) * 65536.0));
      wl10 = fmax(wl10, fabs(fm_log10_q16(x) - log10(v) * 65536.0));
      wdb = fmax(wdb, fabs(fm_db_q16(x) - 20.0 * log10(v) * 65536.0));
    }
    we2 = fmax(we2, fabs(fm_exp2_q16(x) - sat(exp2(v) * 65536.0)));
    we = fmax(we, fabs(fm_exp_q16(x) - sat(exp(v) * 65536.0)));

    /* pow: every magnitude of x, exponents up to +-128 */
    uint64_t h = mix((uint64_t)i);
    int32_t px = (int32_t)((uint32_t)h >> 1) >> ((h >> 32) & 15U);
    int32_t py = (int32_t)(uint32_t)(h >> 32) >> (8U + ((h >> 60) & 15U));
    if (px > 0 && py != 0) {
      double r = sat(pow(px / 65536.0, py / 65536.0) * 65536.0);
      double bound = 0.65 + fabs(py / 65536.0) * r / 4294967296.0;
      wp = fmax(wp, fabs(fm_pow_q16(px, py) - r) / bound);
    }
  }
  printf("exhaustive: log2 %.4f, ln %.4f, log10 %.4f, db %.4f LSB (all 2^31 "
         "positive); exp2 %.4f, exp %.4f (all 2^32); pow %.3f of its bound "
         "(2^32 hashed pairs)\n",
         wl2, wln, wl10, wdb, we2, we, wp);
  return wl2 <= 0.501 && wln <= 0.501 && wl10 <= 0.501 && wdb <= 0.501 &&
                 we2 <= 0.54 && we <= 0.54 && wp <= 1.0
             ? 0
             : 1;
}
