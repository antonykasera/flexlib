/**
 * @file fd_tone.c
 * @brief Goertzel single-bin detection, NCO and recursive oscillator
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_tone.h"
#include "fd_wide.h"
#include "fm_trig.h"
#include <stddef.h>

/* ---- Goertzel ---- */

void fd_goertzel_init(FD_Goertzel *g, uint32_t freq, uint32_t shift) {
  FX_ASSERT(shift <= 31U);
  g->c = fm_cos_q31(freq); /* 2 cos w in Q2.30 is cos w in q31 */
  fm_sincos_q31(freq, &g->sw, &g->cw);
  g->shift = shift;
  fd_goertzel_reset(g);
}

void fd_goertzel_reset(FD_Goertzel *g) {
  g->s1 = 0;
  g->s2 = 0;
}

/* s0 = v + round(c s1 / 2^30) - s2; wraps past 2^31 (the caller keeps clear) */
FX_INLINE int32_t goertzel_step(int32_t c, int32_t v, int32_t s1, int32_t s2) {
  uint64_t p = (uint64_t)fx_smull(c, s1) + 0x20000000U;
  uint32_t r = ((uint32_t)(p >> 32) << 2) | ((uint32_t)p >> 30);
  return (int32_t)(r + (uint32_t)v - (uint32_t)s2);
}

void fd_goertzel_q15(FD_Goertzel *g, const fm_q15 *x, uint32_t n) {
  FX_ASSERT(g->shift <= 16U);
  const int32_t c = g->c;
  const uint32_t sh = g->shift;
  int32_t s1 = g->s1, s2 = g->s2;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t s0 = goertzel_step(c, x[i] * (1 << sh), s1, s2);
    s2 = s1;
    s1 = s0;
  }
  g->s1 = s1;
  g->s2 = s2;
}

void fd_goertzel_q31(FD_Goertzel *g, const fm_q31 *x, uint32_t n) {
  const int32_t c = g->c;
  const uint32_t sh = g->shift;
  const int64_t half = sh != 0U ? (int64_t)1 << (sh - 1U) : 0;
  int32_t s1 = g->s1, s2 = g->s2;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v = (int32_t)(((int64_t)x[i] + half) >> sh);
    int32_t s0 = goertzel_step(c, v, s1, s2);
    s2 = s1;
    s1 = s0;
  }
  g->s1 = s1;
  g->s2 = s2;
}

/* s1 - e^(-jw) s2: re = s1 - s2 cos w, im = s2 sin w */
void fd_goertzel_dft(const FD_Goertzel *g, int64_t *re, int64_t *im) {
  *re = g->s1 - ((fx_smull(g->s2, g->cw) + 0x40000000) >> 31);
  *im = (fx_smull(g->s2, g->sw) + 0x40000000) >> 31;
}

/* s1^2 + s2^2 - round(c s1 s2 / 2^30), in 96 bits */
uint64_t fd_goertzel_power(const FD_Goertzel *g) {
  const int64_t p = fx_smull(g->s1, g->s2);
  const int64_t ph = p >> 32; /* p = ph 2^32 + pl */
  const uint32_t pl = (uint32_t)p;
  Sum96 s = {0U, 0};
  sum96_add(&s, fx_smull(g->s1, g->s1));
  sum96_add(&s, fx_smull(g->s2, g->s2));
  for (int i = 0; i < 4; i++) { /* ph c 2^32 / 2^30 = 4 ph c, exact */
    sum96_add(&s, -(ph * g->c));
  }
  sum96_add(&s, -(((int64_t)pl * g->c + 0x20000000) >> 30));
  if (s.hi < 0) {
    return 0U;
  }
  return s.hi > 0 ? UINT64_MAX : s.lo;
}

/* ---- NCO ---- */

void fd_nco_init(FD_Nco *o, uint32_t phase, uint32_t inc, int32_t dinc) {
  o->phase = phase;
  o->inc = inc;
  o->dinc = dinc;
}

FX_INLINE fm_q15 q31_to_q15(int32_t v) {
  return fm_sat_q15((int32_t)(((int64_t)v + 0x8000) >> 16));
}

void fd_nco_q31(FD_Nco *o, fm_q31 *s, fm_q31 *c, uint32_t n) {
  uint32_t ph = o->phase, inc = o->inc;
  const uint32_t d = (uint32_t)o->dinc;
  for (uint32_t i = 0U; i < n; i++) {
    if (s != NULL && c != NULL) {
      fm_sincos_q31(ph, &s[i], &c[i]);
    } else if (s != NULL) {
      s[i] = fm_sin_q31(ph);
    } else if (c != NULL) {
      c[i] = fm_cos_q31(ph);
    }
    ph += inc;
    inc += d;
  }
  o->phase = ph;
  o->inc = inc;
}

void fd_nco_q15(FD_Nco *o, fm_q15 *s, fm_q15 *c, uint32_t n) {
  uint32_t ph = o->phase, inc = o->inc;
  const uint32_t d = (uint32_t)o->dinc;
  for (uint32_t i = 0U; i < n; i++) {
    if (s != NULL && c != NULL) {
      fm_q31 vs, vc;
      fm_sincos_q31(ph, &vs, &vc);
      s[i] = q31_to_q15(vs);
      c[i] = q31_to_q15(vc);
    } else if (s != NULL) {
      s[i] = q31_to_q15(fm_sin_q31(ph));
    } else if (c != NULL) {
      c[i] = q31_to_q15(fm_cos_q31(ph));
    }
    ph += inc;
    inc += d;
  }
  o->phase = ph;
  o->inc = inc;
}

void fd_nco_cplx_q31(FD_Nco *o, fd_cq31 *y, uint32_t n) {
  uint32_t ph = o->phase, inc = o->inc;
  const uint32_t d = (uint32_t)o->dinc;
  for (uint32_t i = 0U; i < n; i++) {
    fm_sincos_q31(ph, &y[i].im, &y[i].re);
    ph += inc;
    inc += d;
  }
  o->phase = ph;
  o->inc = inc;
}

void fd_nco_cplx_q15(FD_Nco *o, fd_cq15 *y, uint32_t n) {
  uint32_t ph = o->phase, inc = o->inc;
  const uint32_t d = (uint32_t)o->dinc;
  for (uint32_t i = 0U; i < n; i++) {
    fm_q31 vs, vc;
    fm_sincos_q31(ph, &vs, &vc);
    fd_cq15 v = {q31_to_q15(vc), q31_to_q15(vs)};
    y[i] = v;
    ph += inc;
    inc += d;
  }
  o->phase = ph;
  o->inc = inc;
}

/* ---- oscillator: Q30 state, renormalised every 256 samples ---- */

#define OSC_RENORM 256U

void fd_osc_init(FD_Osc *o, uint32_t phase, uint32_t inc) {
  fm_q31 s, c;
  fm_sincos_q31(phase, &s, &c);
  o->c = (int32_t)(((int64_t)c + 1) >> 1);
  o->s = (int32_t)(((int64_t)s + 1) >> 1);
  fm_sincos_q31(inc, &o->sw, &o->cw);
  o->count = 0U;
}

/* the amplitude is pulled back to 1 by (3 - |z|^2) / 2 */
FX_INLINE void osc_renorm(int32_t *c, int32_t *s) {
  int64_t m = fx_smull(*c, *c) + fx_smull(*s, *s);   /* about 2^60 */
  int32_t gain = (int32_t)(((3LL << 60) - m) >> 31); /* Q30 */
  *c = (int32_t)((fx_smull(*c, gain) + 0x20000000) >> 30);
  *s = (int32_t)((fx_smull(*s, gain) + 0x20000000) >> 30);
}

/* c' = c cos w - s sin w, s' = s cos w + c sin w, in runs up to each
 * renormalisation; the outputs given as constants fold away */
FX_INLINE void osc_run(FD_Osc *o, uint32_t n, fm_q31 *s31, fm_q31 *c31,
                       fm_q15 *s15, fm_q15 *c15) {
  const int32_t cw = o->cw, sw = o->sw;
  int32_t vc = o->c, vs = o->s;
  uint32_t cnt = o->count, i = 0U;
  while (i < n) {
    uint32_t end = n - i < OSC_RENORM - cnt ? n : i + (OSC_RENORM - cnt);
    cnt += end - i;
    for (; i < end; i++) {
      if (s31 != NULL) {
        s31[i] = fm_sat_q31((int64_t)vs * 2);
      }
      if (c31 != NULL) {
        c31[i] = fm_sat_q31((int64_t)vc * 2);
      }
      if (s15 != NULL) {
        s15[i] = fm_sat_q15((vs + 0x4000) >> 15);
      }
      if (c15 != NULL) {
        c15[i] = fm_sat_q15((vc + 0x4000) >> 15);
      }
      int32_t cn =
          (int32_t)((fx_smull(vc, cw) - fx_smull(vs, sw) + 0x40000000) >> 31);
      vs = (int32_t)((fx_smull(vs, cw) + fx_smull(vc, sw) + 0x40000000) >> 31);
      vc = cn;
    }
    if (cnt == OSC_RENORM) {
      osc_renorm(&vc, &vs);
      cnt = 0U;
    }
  }
  o->c = vc;
  o->s = vs;
  o->count = cnt;
}

void fd_osc_q31(FD_Osc *o, fm_q31 *s, fm_q31 *c, uint32_t n) {
  if (s != NULL && c != NULL) {
    osc_run(o, n, s, c, NULL, NULL);
  } else if (s != NULL) {
    osc_run(o, n, s, NULL, NULL, NULL);
  } else if (c != NULL) {
    osc_run(o, n, NULL, c, NULL, NULL);
  } else {
    osc_run(o, n, NULL, NULL, NULL, NULL);
  }
}

void fd_osc_q15(FD_Osc *o, fm_q15 *s, fm_q15 *c, uint32_t n) {
  if (s != NULL && c != NULL) {
    osc_run(o, n, NULL, NULL, s, c);
  } else if (s != NULL) {
    osc_run(o, n, NULL, NULL, s, NULL);
  } else if (c != NULL) {
    osc_run(o, n, NULL, NULL, NULL, c);
  } else {
    osc_run(o, n, NULL, NULL, NULL, NULL);
  }
}
