/**
 * @file fd_filt.c
 * @brief Small filters: DC blocker, one-pole smoother, moving average
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_filt.h"
#include "fm_float.h"

/* ---- DC blocker: x - x1 is an integer, so adding the rounded a y1 rounds
 * the exact value once ---- */

void fd_dcblock_init_q15(FD_DcBlockQ15 *d, fm_q15 a) {
  d->a = a;
  d->x1 = 0;
  d->y1 = 0;
}

void fd_dcblock_init_q31(FD_DcBlockQ31 *d, fm_q31 a) {
  d->a = a;
  d->x1 = 0;
  d->y1 = 0;
}

void fd_dcblock_q15(FD_DcBlockQ15 *d, const fm_q15 *x, fm_q15 *y, uint32_t n) {
  const int32_t a = d->a;
  int32_t x1 = d->x1, y1 = d->y1;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t x0 = x[i];
    y1 = fm_sat_q15(x0 - x1 + ((a * y1 + 0x4000) >> 15));
    x1 = x0;
    y[i] = (fm_q15)y1;
  }
  d->x1 = (fm_q15)x1;
  d->y1 = (fm_q15)y1;
}

void fd_dcblock_q31(FD_DcBlockQ31 *d, const fm_q31 *x, fm_q31 *y, uint32_t n) {
  const int32_t a = d->a;
  int32_t x1 = d->x1, y1 = d->y1;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t x0 = x[i];
    int64_t v = (int64_t)x0 - x1 + (fx_smlal(0x40000000, a, y1) >> 31);
    y1 = fm_sat_q31(v);
    x1 = x0;
    y[i] = y1;
  }
  d->x1 = x1;
  d->y1 = y1;
}

/* ---- one pole: y moves toward x by a fraction a < 1, never past it ---- */

void fd_onepole_init_q15(FD_OnePoleQ15 *p, fm_q15 a, fm_q15 y0) {
  FX_ASSERT(a >= 0);
  p->a = a;
  p->y = y0;
}

void fd_onepole_init_q31(FD_OnePoleQ31 *p, fm_q31 a, fm_q31 y0) {
  FX_ASSERT(a >= 0);
  p->a = a;
  p->y = y0;
}

void fd_onepole_q15(FD_OnePoleQ15 *p, const fm_q15 *x, fm_q15 *y, uint32_t n) {
  const int32_t a = p->a;
  int32_t v = p->y;
  for (uint32_t i = 0U; i < n; i++) {
    v += (a * (x[i] - v) + 0x4000) >> 15; /* |a (x - y)| < 2^31 - 2^16 */
    y[i] = (fm_q15)v;
  }
  p->y = (fm_q15)v;
}

void fd_onepole_q31(FD_OnePoleQ31 *p, const fm_q31 *x, fm_q31 *y, uint32_t n) {
  const int32_t a = p->a;
  int32_t v = p->y;
  for (uint32_t i = 0U; i < n; i++) {
    int64_t d = fx_smull(a, x[i]) - fx_smull(a, v) + 0x40000000;
    v = (int32_t)(v + (d >> 31)); /* the step alone can pass 2^31 */
    y[i] = v;
  }
  p->y = v;
}

/* |H| = 1/sqrt 2 at fc: a = sqrt(c^2 + 2c) - c with c = 1 - cos w */
static float onepole_coef(float fc) {
  float s = fm_sinf(3.14159265358979f * fc);
  float c = 2.0f * s * s;
  return fm_sqrtf(c * c + 2.0f * c) - c;
}

fm_q15 fd_onepole_coef_q15(float fc) {
  FX_ASSERT(fc > 0.0f && fc < 0.5f);
  return fm_f32_to_q15(onepole_coef(fc));
}

fm_q31 fd_onepole_coef_q31(float fc) {
  FX_ASSERT(fc > 0.0f && fc < 0.5f);
  return fm_f32_to_q31(onepole_coef(fc));
}

/* ---- moving average: an exact running sum ---- */

static uint32_t pow2_shift(uint32_t n) {
  return (n & (n - 1U)) == 0U ? fx_ctz(n) : 32U;
}

void fd_movavg_init_q15(FD_MovAvgQ15 *m, fm_q15 *buf, uint32_t n) {
  FX_ASSERT(n >= 1U && n <= 65536U);
  m->buf = buf;
  m->n = n;
  m->pos = 0U;
  m->shift = pow2_shift(n);
  m->sum = 0;
  for (uint32_t i = 0U; i < n; i++) {
    buf[i] = 0;
  }
}

void fd_movavg_init_q31(FD_MovAvgQ31 *m, fm_q31 *buf, uint32_t n) {
  FX_ASSERT(n >= 1U);
  m->buf = buf;
  m->n = n;
  m->pos = 0U;
  m->shift = pow2_shift(n);
  m->sum = 0;
  for (uint32_t i = 0U; i < n; i++) {
    buf[i] = 0;
  }
}

void fd_movavg_q15(FD_MovAvgQ15 *m, const fm_q15 *x, fm_q15 *y, uint32_t n) {
  const uint32_t len = m->n, k = m->shift;
  const int32_t half = (int32_t)(len >> 1);
  uint32_t pos = m->pos;
  int32_t sum = m->sum;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t x0 = x[i];
    sum += x0 - m->buf[pos];
    m->buf[pos] = (fm_q15)x0;
    pos = pos + 1U == len ? 0U : pos + 1U;
    int32_t q;
    if (k != 32U) {
      q = (sum + half) >> k;
    } else { /* floor, then up when the remainder is at least half */
      q = sum / (int32_t)len;
      int32_t r = sum - q * (int32_t)len;
      if (r < 0) {
        r += (int32_t)len;
        q--;
      }
      q += 2U * (uint32_t)r >= len ? 1 : 0;
    }
    y[i] = (fm_q15)q;
  }
  m->pos = pos;
  m->sum = sum;
}

void fd_movavg_q31(FD_MovAvgQ31 *m, const fm_q31 *x, fm_q31 *y, uint32_t n) {
  const uint32_t len = m->n, k = m->shift;
  uint32_t pos = m->pos;
  int64_t sum = m->sum;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t x0 = x[i];
    sum += (int64_t)x0 - m->buf[pos];
    m->buf[pos] = x0;
    pos = pos + 1U == len ? 0U : pos + 1U;
    int64_t q;
    if (k != 32U) {
      q = (sum + (int64_t)(len >> 1)) >> k;
    } else {
      q = sum / (int64_t)len;
      int64_t r = sum - q * (int64_t)len;
      if (r < 0) {
        r += len;
        q--;
      }
      q += 2 * r >= (int64_t)len ? 1 : 0;
    }
    y[i] = (fm_q31)q;
  }
  m->pos = pos;
  m->sum = sum;
}
