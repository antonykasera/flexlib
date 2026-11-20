/**
 * @file exhaustive_fd_bfp_slow.c
 * @brief Block floating point conversions on every float and every q31, at
 * block exponents from the value's own to far above it
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_bfp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHUNK 4096

static const int32_t q_up[] = {0, 1, 7, 8, 24, 25, 40};
static const int32_t f_exp[] = {-170, -150, -140, -110, -1, 31, 127, 128, 160};

int main(void) {
  unsigned long long bad_exp = 0U, bad_q = 0U, bad_f = 0U;

#pragma omp parallel for reduction(+ : bad_exp, bad_q, bad_f) schedule(dynamic)
  for (long long c = 0; c < (1LL << 32) / CHUNK; c++) {
    float x[CHUNK];
    int32_t q[CHUNK], qv[CHUNK];
    float y[CHUNK];
    for (int i = 0; i < CHUNK; i++) {
      uint32_t u = (uint32_t)(c * CHUNK + i);
      memcpy(&x[i], &u, sizeof u);
      qv[i] = (int32_t)u;
    }

    /* x -> q31 at the value's own exponent and above */
    for (int i = 0; i < CHUNK; i++) {
      int32_t e0 = fd_bfp_exp(&x[i], 1U);
      if (!isfinite(x[i])) {
        bad_exp += e0 != FD_BFP_BAD;
        continue;
      }
      int k = 0;
      (void)frexp((double)x[i], &k);
      int32_t want = x[i] == 0.0f ? -125 : (k < -125 ? -125 : k);
      bad_exp += e0 != want;
      for (unsigned j = 0U; j < sizeof q_up / sizeof q_up[0]; j++) {
        int32_t e = e0 + q_up[j];
        int32_t got;
        fd_bfp_to_q31(&x[i], &got, 1U, e);
        double r = nearbyint(ldexp((double)x[i], 31 - e));
        bad_q += (double)got != r;
      }
    }

    /* q31 -> float, a whole chunk per call, then in place */
    for (unsigned j = 0U; j < sizeof f_exp / sizeof f_exp[0]; j++) {
      int32_t e = f_exp[j];
      double scale = ldexp(1.0, e - 31);
      fd_bfp_to_f32(qv, y, CHUNK, e);
      memcpy(q, qv, sizeof q);
      fd_bfp_to_f32(q, (float *)(void *)q, CHUNK, e);
      for (int i = 0; i < CHUNK; i++) {
        float want = (float)((double)qv[i] * scale);
        bad_f += memcmp(&y[i], &want, sizeof want) != 0;
        bad_f += memcmp(&q[i], &want, sizeof want) != 0;
      }
    }
  }

  printf("fd_bfp_exp:    %llu wrong of 2^32\n", bad_exp);
  printf("fd_bfp_to_q31: %llu wrong of 7 x 2^32 (finite)\n", bad_q);
  printf("fd_bfp_to_f32: %llu wrong of 2 x 9 x 2^32\n", bad_f);
  return bad_exp + bad_q + bad_f == 0U ? 0 : 1;
}
