/**
 * @file exhaustive_atan.c
 * @brief fm_atan2_q15 on every pair, fm_asin_q31 / fm_acos_q31 on every input,
 *        fm_atan2_q31 on 2^32 hashed pairs (`make exhaustive`)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_atan.h"
#include <math.h>
#include <stdio.h>

/* got - ref in LSB, mod a turn; in double the difference keeps 2^-22 LSB */
static double ang_err(double got, double ref_rad, double turn) {
  double e = got - ref_rad * (turn / 6.283185307179586);
  e -= turn * nearbyint(e / turn);
  return fabs(e);
}

static uint64_t mix(uint64_t z) {
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

int main(void) {
  double w15 = 0.0, w31 = 0.0, ws = 0.0;

#pragma omp parallel for reduction(max : w15, w31, ws) schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t u = (uint32_t)i;
    int32_t y15 = (int16_t)(u >> 16);
    int32_t x15 = (int16_t)u;
    double e = ang_err(fm_atan2_q15((fm_q15)y15, (fm_q15)x15),
                       atan2((double)y15, (double)x15), 65536.0);
    w15 = e > w15 ? e : w15;

    double z = (int32_t)u / 2147483648.0; /* exact; double is 1e-7 LSB */
    e = ang_err(fm_asin_q31((int32_t)u), asin(z), 4294967296.0);
    double ec = ang_err(fm_acos_q31((int32_t)u), acos(z), 4294967296.0);
    ws = e > ws ? e : ws;
    ws = ec > ws ? ec : ws;

    /* every magnitude, the diagonal, and ratios just below each k / 128 */
    uint64_t h = mix((uint64_t)i);
    int32_t y = (int32_t)(uint32_t)h >> ((h >> 32) & 31U);
    int32_t x = (int32_t)(uint32_t)(h >> 37) >> ((h >> 59) & 31U);
    if ((h & 0x30000000U) == 0x10000000U) {
      x = (int32_t)((uint32_t)y + ((h >> 8) & 0xFU) - 8U);
    } else if ((h & 0x30000000U) == 0x20000000U) {
      x = (int32_t)((uint32_t)(h >> 32) | 0x40000000U) >> 1;
      y = (int32_t)((int64_t)x * (int32_t)((h >> 8) & 127U) / 128) -
          (int32_t)((h >> 16) & 3U);
    }
    e = ang_err(fm_atan2_q31(y, x), atan2((double)y, (double)x), 4294967296.0);
    w31 = e > w31 ? e : w31;
  }
  printf("exhaustive: fm_atan2_q15 max %.4f LSB (all 2^32 pairs), fm_asin_q31 "
         "/ fm_acos_q31 %.4f (all 2^32), fm_atan2_q31 %.4f (2^32 hashed "
         "pairs)\n",
         w15, ws, w31);
  return w15 <= 0.65 && ws <= 0.81 && w31 <= 0.65 ? 0 : 1;
}
