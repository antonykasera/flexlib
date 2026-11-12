/**
 * @file fd_noise.c
 * @brief Uniform, Gaussian and triangular (dither) noise
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_noise.h"
#include "fm_exp.h"
#include "fm_sqrt.h"
#include "fm_trig.h"

void fd_noise_uniform_q15(FU_Xoshiro128 *r, fm_q15 *y, uint32_t n) {
  for (uint32_t i = 0U; i + 1U < n; i += 2U) {
    uint32_t u = fu_xoshiro128ss(r);
    y[i] = (fm_q15)(int16_t)u;
    y[i + 1U] = (fm_q15)(int16_t)(u >> 16);
  }
  if ((n & 1U) != 0U) {
    y[n - 1U] = (fm_q15)(int16_t)fu_xoshiro128ss(r);
  }
}

void fd_noise_uniform_q31(FU_Xoshiro128 *r, fm_q31 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = (fm_q31)fu_xoshiro128ss(r);
  }
}

/* a pair of N(0, 1) in Q16.16: sqrt(-2 ln u) (cos, sin)(2 pi v), u in (0, 1) */
static void gauss_pair(FU_Xoshiro128 *r, int32_t *g1, int32_t *g2) {
  uint32_t m = fu_xoshiro128ss(r) >> 1; /* u = m / 2^31 */
  m = m != 0U ? m : 1U;
  int32_t l2 = fm_log2_q16((fm_q16)m) + (16 << 16);    /* log2 m */
  int64_t t = (int64_t)(31 << 16) - l2;                /* -log2 u */
  int32_t big = (int32_t)((t * 90852 + 0x8000) >> 16); /* x 2 ln 2 */
  int32_t rad = fm_sqrt_q16(big);                      /* Q16.16 */
  fm_q31 s, c;
  fm_sincos_q31(fu_xoshiro128ss(r), &s, &c);
  *g1 = (int32_t)((fx_smull(rad, c) + 0x40000000) >> 31);
  *g2 = (int32_t)((fx_smull(rad, s) + 0x40000000) >> 31);
}

void fd_noise_gauss_q15(FU_Xoshiro128 *r, fm_q15 sigma, fm_q15 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i += 2U) {
    int32_t g[2];
    gauss_pair(r, &g[0], &g[1]);
    for (uint32_t j = 0U; j < 2U && i + j < n; j++) {
      int64_t v = ((int64_t)g[j] * sigma + 0x8000) >> 16;
      y[i + j] = v > 32767 ? 32767 : (v < -32768 ? -32768 : (fm_q15)v);
    }
  }
}

void fd_noise_gauss_q31(FU_Xoshiro128 *r, fm_q31 sigma, fm_q31 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i += 2U) {
    int32_t g[2];
    gauss_pair(r, &g[0], &g[1]);
    for (uint32_t j = 0U; j < 2U && i + j < n; j++) {
      y[i + j] = fm_sat_q31((fx_smull(g[j], sigma) + 0x8000) >> 16);
    }
  }
}

void fd_noise_tpdf_q15(FU_Xoshiro128 *r, uint32_t bits, fm_q15 *y, uint32_t n) {
  FX_ASSERT(bits <= 15U);
  const uint32_t mask = (1U << bits) - 1U;
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t u = fu_xoshiro128ss(r);
    y[i] = (fm_q15)((int32_t)(u & mask) - (int32_t)((u >> 16) & mask));
  }
}
