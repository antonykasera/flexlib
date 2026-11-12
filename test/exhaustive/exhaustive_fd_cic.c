/**
 * @file exhaustive_fd_cic.c
 * @brief The CIC decimator against cascaded moving sums: orders 1-4, ratios
 * 2-16, every input width, 10^6 blocks cut at random
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_resample.h"
#include <stdio.h>
#include <string.h>

__extension__ typedef __int128 i128;

#define NMAX 200U

static uint32_t next(uint64_t *s) {
  *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
  return (uint32_t)(*s >> 32);
}

int main(void) {
  unsigned long long bad = 0U, outs = 0U;
#pragma omp parallel reduction(+ : bad, outs)
  {
    int32_t x[NMAX], got[NMAX], want[NMAX];
    fm_q15 xs[NMAX];
    i128 s[5][NMAX];
#pragma omp for schedule(static)
    for (int blk = 0; blk < 1000000; blk++) {
      uint64_t r = 0x9E3779B97F4A7C15U * (uint64_t)(uint32_t)(blk + 1);
      uint32_t order = 1U + next(&r) % 4U, ratio = 2U + next(&r) % 15U;
      uint32_t bits = 1U + next(&r) % 32U;
      FD_Cic c;
      if (fd_cic_init(&c, order, ratio, bits) != FD_OK) {
        continue;
      }
      uint32_t n = ratio * (1U + next(&r) % 8U) + next(&r) % ratio;
      n = n > NMAX ? NMAX : n;
      const int64_t hi = (int64_t)(1ULL << (bits - 1U)) - 1, lo = -hi - 1;
      int kind = (int)(next(&r) % 5U);
      for (uint32_t i = 0U; i < n; i++) {
        uint32_t u = next(&r);
        int64_t v = kind == 0   ? lo + (int64_t)(u % (uint64_t)(hi - lo + 1))
                    : kind == 1 ? hi
                    : kind == 2 ? lo
                    : kind == 3 ? ((u & 1U) != 0U ? hi : lo)
                                : ((i / ratio) % 2U == 0U ? hi : lo);
        x[i] = (int32_t)v;
        xs[i] = (fm_q15)v;
      }
      /* the oracle: order moving sums of ratio, sampled after every
       * ratio-th input, aligned to q31 */
      for (uint32_t i = 0U; i < n; i++) {
        s[0][i] = x[i];
      }
      for (uint32_t o = 1U; o <= order; o++) {
        i128 run = 0;
        for (uint32_t i = 0U; i < n; i++) {
          run += s[o - 1U][i] - (i >= ratio ? s[o - 1U][i - ratio] : 0);
          s[o][i] = run;
        }
      }
      uint32_t b = bits + c.growth, nw = 0U;
      for (uint32_t i = ratio - 1U; i < n; i += ratio) {
        i128 v = s[order][i];
        if (b <= 32U) {
          want[nw++] = (int32_t)(v * ((i128)1 << (32U - b)));
        } else {
          i128 q = (v + ((i128)1 << (b - 33U))) >> (b - 32U);
          want[nw++] = q > INT32_MAX ? INT32_MAX : (int32_t)q;
        }
      }
      uint32_t ng = 0U;
      for (uint32_t i = 0U; i < n;) {
        uint32_t q = 1U + next(&r) % (2U * ratio);
        q = q > n - i ? n - i : q;
        ng += bits <= 16U && (blk & 1) != 0
                  ? fd_cic_decim_q15(&c, xs + i, got + ng, q)
                  : fd_cic_decim_q31(&c, x + i, got + ng, q);
        i += q;
      }
      outs += nw;
      bad += ng != nw || memcmp(got, want, nw * sizeof got[0]) != 0;
    }
  }
  printf("exhaustive: fd_cic_decim %llu bad blocks of 10^6 (orders 1-4, ratios "
         "2-16, %llu outputs)\n",
         bad, outs);
  return bad == 0U ? 0 : 1;
}
