/**
 * @file exhaustive_trig.c
 * @brief fm_sin_q31 on every 32-bit angle against sinl
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fm_trig.h"
#include <math.h>
#include <stdio.h>

int main(void) {
  const long double step = 6.283185307179586476925286766559L / 4294967296.0L;
  double worst = 0.0;
  unsigned long long bad = 0U;

#pragma omp parallel for reduction(max : worst) reduction(+ : bad)             \
    schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t a = (uint32_t)i;
    long double ref = sinl(step * (long double)a) * 2147483648.0L;
    if (ref > 2147483647.0L) {
      ref = 2147483647.0L;
    }
    fm_q31 s = fm_sin_q31(a);
    double e = fabs((double)((long double)s - ref));
    worst = e > worst ? e : worst;
    fm_q31 ss, cc;
    fm_sincos_q31(a - 0x40000000U, &ss, &cc);
    bad += cc != s || fm_cos_q31(a - 0x40000000U) != s;
  }
  printf("exhaustive: fm_sin_q31 max error %.4f LSB over all 2^32 angles; "
         "cos / sincos %llu mismatches\n",
         worst, bad);
  return worst <= 1.11 && bad == 0U ? 0 : 1;
}
