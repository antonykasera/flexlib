/**
 * @file fd_fir_f32.c
 * @brief Float FIR on the q31 kernel: block floating point in, one rounding out
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_bfp.h"
#include "fd_fir_core.h"
#include <math.h>
#include <string.h>

/* the smallest e with sum |hq| < 2: the narrow kernel */
void fd_fir_init_f32(FD_FirF32 *f, const float *h, fm_q31 *hq, uint32_t ntaps,
                     float *xs, fm_q31 *state, uint32_t state_len) {
  int32_t e = fd_bfp_exp(h, ntaps);
  FX_ASSERT(e != FD_BFP_BAD);
  for (;;) {
    fd_bfp_to_q31(h, hq, ntaps, e);
    if (fd_fir_gain_q31(hq, ntaps) < (2 << 16)) {
      break;
    }
    e++;
  }
  f->eh = e;
  f->xs = xs;
  fd_fir_init_q31(&f->fir, hq, ntaps, state, state_len);
  fd_fir_reset_f32(f);
}

void fd_fir_reset_f32(FD_FirF32 *f) {
  fd_fir_reset_q31(&f->fir);
  for (uint32_t i = 0U; i < f->fir.len; i++) {
    f->xs[i] = 0.0f;
  }
  f->es = -125;
  f->since = 0U;
  f->poison = 0U;
}

static void movef(float *d, const float *s, uint32_t n) {
  FX_OPAQUE(d);
  for (uint32_t i = 0U; i < n; i++) {
    d[i] = s[i];
  }
}

/* E falls only after th quiet inputs; the history is reconverted from xs */
static void set_exp(FD_FirF32 *f, uint32_t w, uint32_t th, int32_t ec,
                    uint32_t m) {
  int32_t e = f->es;
  if (ec >= e) {
    e = ec;
    f->since = 0U;
  } else if (f->since >= th) {
    int32_t eh = fd_bfp_exp(f->xs + w - th, th);
    e = eh > ec ? eh : ec;
    f->since = 0U;
  } else {
    f->since += m;
  }
  if (e != f->es) {
    f->es = e;
    fd_bfp_to_q31(f->xs + w - th, f->fir.state + w - th, th, e);
  }
}

static void fir_run(FD_FirF32 *f, const float *x, float *y, uint32_t m,
                    int32_t ec) {
  FD_FirQ31 *q = &f->fir;
  const uint32_t t = q->ntaps;
  movef(f->xs + q->w, x, m);
  set_exp(f, q->w, t - 1U, ec, m);
  fd_bfp_to_q31(f->xs + q->w, q->state + q->w, m, f->es);
  k31(q->h, 1U, false, t, q->state + q->w - (t - 1U), NULL, 1U, m, 0U, 0U, 0U,
      NULL, 0, y, f->es + f->eh - 62);
  q->w += m;
}

/* bits: no soft-float compare */
static FX_COLD uint32_t first_bad(const float *x, uint32_t m) {
  uint32_t i = 0U;
  for (; i < m; i++) {
    uint32_t b;
    memcpy(&b, &x[i], sizeof b);
    if ((b & 0x7F800000U) == 0x7F800000U) {
      break;
    }
  }
  return i;
}

static FX_COLD uint32_t spoil(float *y, uint32_t m, uint32_t p) {
  uint32_t k = p < m ? p : m;
  for (uint32_t i = 0U; i < k; i++) {
    y[i] = NAN;
  }
  return p - k;
}

void fd_fir_f32(FD_FirF32 *f, const float *x, float *y, uint32_t n) {
  FD_FirQ31 *q = &f->fir;
  const uint32_t t = q->ntaps;
  while (n != 0U) {
    if (q->w == q->len) {
      move31(q->state, q->state + q->w - (t - 1U), t - 1U);
      movef(f->xs, f->xs + q->w - (t - 1U), t - 1U);
      q->w = t - 1U;
    }
    uint32_t m = q->len - q->w < n ? q->len - q->w : n;
    int32_t ec = fd_bfp_exp(x, m);
    if (FX_UNLIKELY(ec == FD_BFP_BAD)) {
      uint32_t j = first_bad(x, m);
      if (j == 0U) {
        const float zero = 0.0f;
        fir_run(f, &zero, y, 1U, -125);
        f->poison = t;
        m = 1U;
      } else {
        m = j;
        fir_run(f, x, y, m, fd_bfp_exp(x, m));
      }
    } else {
      fir_run(f, x, y, m, ec);
    }
    if (FX_UNLIKELY(f->poison != 0U)) {
      f->poison = spoil(y, m, f->poison);
    }
    x += m;
    y += m;
    n -= m;
  }
}

float fd_fir_sample_f32(FD_FirF32 *f, float x) {
  float y;
  fd_fir_f32(f, &x, &y, 1U);
  return y;
}
