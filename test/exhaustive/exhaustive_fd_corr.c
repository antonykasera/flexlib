/**
 * @file exhaustive_fd_corr.c
 * @brief Convolution and correlation at every size to 16 against exact sums,
 * 10^6 blocks; the peak vertex for every pair of neighbour drops (2^32)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_corr.h"
#include <stdio.h>
#include <string.h>

__extension__ typedef __int128 i128;

#define NMAX 16

static uint32_t next(uint64_t *s) {
  *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
  return (uint32_t)(*s >> 32);
}

static i128 rdiv(i128 v, unsigned s) {
  return (v + ((i128)1 << (s - 1U))) >> s;
}

static int32_t clamp(i128 v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : (int32_t)v);
}

/* 0 full scale, 1 sum |x| < 2 to 64 terms, 2 +-full scale, 3 12 (q15) or 28
 * (q31) bits, 4 most negative; q15 is the q31 value's top half */
static int32_t sample(uint64_t *s, int kind, int q31) {
  uint32_t u = next(s);
  int32_t v = kind == 0   ? (int32_t)u
              : kind == 1 ? (int32_t)u / 70
              : kind == 2 ? ((u & 1U) != 0U ? INT32_MAX : INT32_MIN)
              : kind == 3 ? (int32_t)u >> 4
                          : INT32_MIN;
  return q31 ? v : v >> 16;
}

/* round((yp - ym) 2^16 / (2 (2 y0 - ym - yp))) to nearest, ties up */
static int32_t ref_vertex(int64_t ym, int64_t y0, int64_t yp) {
  i128 num = (i128)(yp - ym) * 65536, den = 2 * (2 * (i128)y0 - ym - yp);
  if (den <= 0) {
    return 0;
  }
  i128 t = 2 * num + den, q = t / (2 * den);
  return (int32_t)(q * 2 * den > t ? q - 1 : q);
}

int main(void) {
  unsigned long long bad15 = 0U, bad31 = 0U, badpk = 0U;
#pragma omp parallel reduction(+ : bad15, bad31)
  {
    int32_t wa[NMAX], wb[NMAX];
    fm_q15 a[NMAX], b[NMAX], y[2 * NMAX + 8];
    fm_q31 a31[NMAX], b31[NMAX], y31[2 * NMAX + 8];
#pragma omp for schedule(static)
    for (int blk = 0; blk < 1000000; blk++) {
      uint64_t s = 0x9E3779B97F4A7C15U * (uint64_t)(uint32_t)(blk + 1);
      uint32_t na = 1U + (uint32_t)blk % NMAX,
               nb = 1U + (uint32_t)blk / NMAX % NMAX;
      int ka = (int)(next(&s) % 5U), kb = (int)(next(&s) % 5U);
      uint32_t sh = next(&s) % 4U == 0U ? next(&s) % 17U : 0U;
      for (uint32_t i = 0U; i < NMAX; i++) {
        wa[i] = sample(&s, ka, 0);
        wb[i] = sample(&s, kb, 0);
        a[i] = (fm_q15)wa[i];
        b[i] = (fm_q15)wb[i];
        a31[i] = sample(&s, ka, 1);
        b31[i] = sample(&s, kb, 1);
      }
      fd_conv_q15(a, na, b, nb, sh, y);
      fd_conv_q31(a31, na, b31, nb, 2U * sh, y31);
      for (uint32_t k = 0U; k + 1U < na + nb; k++) {
        i128 v = 0, v31 = 0;
        for (uint32_t i = 0U; i < na; i++) {
          if (k >= i && k - i < nb) {
            v += (i128)wa[i] * wb[k - i];
            v31 += (i128)a31[i] * b31[k - i];
          }
        }
        bad15 += y[k] != clamp(rdiv(v, 15U + sh), -32768, 32767);
        bad31 +=
            y31[k] != clamp(rdiv(v31, 31U + 2U * sh), INT32_MIN, INT32_MAX);
      }
      int32_t lag0 = -(int32_t)nb - 2 + (int32_t)(next(&s) % 4U);
      uint32_t nl = na + nb + 4U;
      fd_corr_q15(a, na, b, nb, lag0, nl, sh, y);
      fd_corr_q31(a31, na, b31, nb, lag0, nl, 2U * sh, y31);
      for (uint32_t i = 0U; i < nl; i++) {
        i128 v = 0, v31 = 0;
        for (uint32_t j = 0U; j < nb; j++) {
          int32_t p = (int32_t)j + lag0 + (int32_t)i;
          if (p >= 0 && p < (int32_t)na) {
            v += (i128)wa[p] * wb[j];
            v31 += (i128)a31[p] * b31[j];
          }
        }
        bad15 += y[i] != clamp(rdiv(v, 15U + sh), -32768, 32767);
        bad31 +=
            y31[i] != clamp(rdiv(v31, 31U + 2U * sh), INT32_MIN, INT32_MAX);
      }
    }
  }
  /* the vertex depends on the two drops from the maximum only */
#pragma omp parallel for schedule(static) reduction(+ : badpk)
  for (int64_t dm = 0; dm < 65536; dm++) {
    fm_q15 t[3];
    t[0] = (fm_q15)(32767 - dm);
    t[1] = 32767;
    for (int64_t dp = 0; dp < 65536; dp++) {
      t[2] = (fm_q15)(32767 - dp);
      int32_t want =
          dm == 0 ? 0 : 65536 + ref_vertex(32767 - dm, 32767, 32767 - dp);
      badpk += fd_peak_q15(t, 3U, NULL) != want;
    }
  }
  printf("exhaustive: fd_conv / fd_corr q15 %llu bad, q31 %llu (10^6 blocks to "
         "16 x 16); fd_peak_q15 %llu of 2^32 neighbour pairs\n",
         bad15, bad31, badpk);
  return (bad15 | bad31 | badpk) == 0U ? 0 : 1;
}
