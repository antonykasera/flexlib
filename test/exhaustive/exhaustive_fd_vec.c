/**
 * @file exhaustive_fd_vec.c
 * @brief fd_vec q15 element-wise ops on all 2^32 pairs, q31 to q15 on all 2^32
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_vec.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define N 65536U

static int32_t clamp15(int64_t v) {
  return v < -32768 ? -32768 : (v > 32767 ? 32767 : (int32_t)v);
}

/* round(v / 2^s), ties up */
static int64_t rdiv(int64_t v, unsigned s) {
  return (v + (1LL << (s - 1U))) >> s;
}

static uint32_t ref_mag(uint64_t s) {
  uint64_t r = (uint64_t)sqrtl((long double)s);
  while (r * r > s) {
    r--;
  }
  while ((r + 1U) * (r + 1U) <= s) {
    r++;
  }
  return (uint32_t)(4U * s > (2U * r + 1U) * (2U * r + 1U) ? r + 1U : r);
}

int main(void) {
  unsigned long long badd = 0U, bsub = 0U, bmul = 0U, bmag = 0U, bpow = 0U,
                     bcmul = 0U, bconv = 0U;

#pragma omp parallel reduction(+ : badd, bsub, bmul, bmag, bpow, bcmul, bconv)
  {
    fm_q15 *a = malloc(N * sizeof *a), *b = malloc(N * sizeof *b),
           *y = malloc(N * sizeof *y);
    fd_cq15 *c = malloc(N * sizeof *c), *cy = malloc(N * sizeof *cy);
    uint16_t *m = malloc(N * sizeof *m);
    uint32_t *p = malloc(N * sizeof *p);
    fm_q31 *w = malloc(N * sizeof *w);
    for (uint32_t j = 0U; j < N; j++) {
      b[j] = (fm_q15)(int32_t)(j - 32768U);
    }

#pragma omp for schedule(static)
    for (long long i = 0; i < (long long)N; i++) {
      int32_t av = (int32_t)i - 32768;
      for (uint32_t j = 0U; j < N; j++) {
        a[j] = (fm_q15)av;
        c[j].re = (fm_q15)av;
        c[j].im = b[j];
        w[j] = (fm_q31)(((uint32_t)i << 16) | j);
      }
      fd_add_q15(a, b, y, N);
      for (uint32_t j = 0U; j < N; j++) {
        badd += y[j] != clamp15(av + b[j]);
      }
      fd_sub_q15(a, b, y, N);
      for (uint32_t j = 0U; j < N; j++) {
        bsub += y[j] != clamp15(av - b[j]);
      }
      fd_mul_q15(a, b, y, N);
      for (uint32_t j = 0U; j < N; j++) {
        bmul += y[j] != clamp15(rdiv((int64_t)av * b[j], 15U));
      }
      fd_cmag_q15(c, m, N);
      fd_cmag2_q15(c, p, N);
      for (uint32_t j = 0U; j < N; j++) {
        uint64_t s = (uint64_t)((int64_t)av * av + (int64_t)b[j] * b[j]);
        bmag += m[j] != ref_mag(s);
        bpow += p[j] != s;
      }
      /* (av, b) times (b, av): im = 2 av b reaches 2^31 at -1, -1 */
      for (uint32_t j = 0U; j < N; j++) {
        cy[j].re = b[j];
        cy[j].im = (fm_q15)av;
      }
      fd_cmul_q15(c, cy, cy, N);
      for (uint32_t j = 0U; j < N; j++) {
        int64_t re = (int64_t)av * b[j] - (int64_t)b[j] * av;
        int64_t im = (int64_t)av * av + (int64_t)b[j] * b[j];
        bcmul += cy[j].re != clamp15(rdiv(re, 15U)) ||
                 cy[j].im != clamp15(rdiv(im, 15U));
      }
      fd_q31_to_q15(w, y, N);
      for (uint32_t j = 0U; j < N; j++) {
        bconv += y[j] != clamp15(rdiv(w[j], 16U));
      }
    }
    free(a);
    free(b);
    free(y);
    free(c);
    free(cy);
    free(m);
    free(p);
    free(w);
  }
  printf("exhaustive: fd_add_q15 %llu bad, fd_sub_q15 %llu, fd_mul_q15 %llu, "
         "fd_cmag_q15 %llu, fd_cmag2_q15 %llu, fd_cmul_q15 %llu (all 2^32 "
         "pairs), fd_q31_to_q15 %llu (all 2^32)\n",
         badd, bsub, bmul, bmag, bpow, bcmul, bconv);
  return (badd | bsub | bmul | bmag | bpow | bcmul | bconv) == 0U ? 0 : 1;
}
