/**
 * @file fd_biquad_f32.c
 * @brief Float biquad cascades (DF1): each section's sum exact and rounded once
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_bfp.h"
#include "fd_biquad.h"
#include "fm_sum.h"
#include <math.h>
#include <string.h>

/* mantissa 2^(e - 156): 6 + 6 bits keep 5 products below 2^63 */
#define SH 6U

FX_INLINE int32_t unp(float x, int32_t *e, uint32_t *bad) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return fm_unp_f32(b, SH, e, bad);
}

FX_INLINE int32_t max2(int32_t a, int32_t b) { return a > b ? a : b; }

FX_INLINE int64_t term(int32_t mc, int32_t mv, int32_t e, int32_t em) {
  uint32_t d = (uint32_t)(em - e);
  return fm_asr64(fx_smull(mc, mv), d < 63U ? d : 63U);
}

/* c: b0 b1 b2 a1 a2 as (m, e); xh, yh: the last two (m, e) */
FX_INLINE float section(const int32_t *c, const int32_t *xh, const int32_t *yh,
                        int32_t mx, int32_t ex) {
  int32_t e0 = c[1] + ex, e1 = c[3] + xh[1], e2 = c[5] + xh[3];
  int32_t e3 = c[7] + yh[1], e4 = c[9] + yh[3];
  int32_t em = max2(max2(max2(e0, e1), max2(e2, e3)), e4);
  int64_t s = term(c[0], mx, e0, em) + term(c[2], xh[0], e1, em) +
              term(c[4], xh[2], e2, em) + term(c[6], yh[0], e3, em) +
              term(c[8], yh[2], e4, em);
  return fd_bfp_f32_of_s64(s, em - 312);
}

FX_INLINE void push(int32_t *h, int32_t m, int32_t e) {
  h[2] = h[0];
  h[3] = h[1];
  h[0] = m;
  h[1] = e;
}

void fd_biquad_init_f32(FD_BiquadF32 *f, const float *c, int32_t *cu,
                        uint32_t nsec, int32_t *state) {
  FX_ASSERT(nsec >= 1U);
  uint32_t bad = 0U;
  for (uint32_t i = 0U; i < 5U * nsec; i++) {
    cu[2U * i] = unp(c[i], &cu[2U * i + 1U], &bad);
  }
  FX_ASSERT(bad == 0U);
  f->cu = cu;
  f->state = state;
  f->nsec = nsec;
  fd_biquad_reset_f32(f);
}

/* zeros at the lowest exponent*/
void fd_biquad_reset_f32(FD_BiquadF32 *f) {
  for (uint32_t i = 0U; i < FD_BIQUAD_F32_STATE_LEN(f->nsec); i += 2U) {
    f->state[i] = 0;
    f->state[i + 1U] = -1024;
  }
  f->bad = 0U;
}

void fd_biquad_f32(FD_BiquadF32 *f, const float *x, float *y, uint32_t n) {
  const uint32_t ns = f->nsec;
  int32_t *st = f->state;
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t bad = f->bad;
    int32_t ev;
    int32_t mv = unp(x[i], &ev, &bad);
    float out = 0.0f;
    for (uint32_t k = 0U; k < ns; k++) {
      int32_t *xh = st + 4U * k;
      out = section(f->cu + 10U * k, xh, xh + 4, mv, ev);
      push(xh, mv, ev);
      mv = unp(out, &ev, &bad);
    }
    push(st + 4U * ns, mv, ev);
    if (FX_UNLIKELY(bad != 0U)) {
      f->bad = 1U;
      out = NAN;
    }
    y[i] = out;
  }
}
