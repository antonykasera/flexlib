/**
 * @file fd_fir_core.h
 * @brief FIR kernels, output rounding and state shared by the filters,
 * fd_corr and fd_resample
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_FIR_CORE_H
#define FD_FIR_CORE_H

#include "fd_fir.h"
#include "fd_wide.h"
#include "fm_fixed.h"
#include "fm_float.h"
#include "fx_arch.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- output rounding (ties up), saturated ---- */

/* |a| <= 2^31 - 2^15 (sum |h| < 2) */
FX_INLINE fm_q15 out15(int32_t a) { return fm_sat_q15((a + 0x4000) >> 15); }

FX_INLINE fm_q15 out15w(int64_t a) {
  int64_t r = (a + 0x4000) >> 15;
  return r > 32767 ? 32767 : (r < -32768 ? -32768 : (fm_q15)r);
}

/* |a| <= 2^63 - 2^31 (sum |h| < 2) */
FX_INLINE fm_q31 out31(int64_t a) {
  uint64_t u = (uint64_t)a + 0x40000000U;
  int32_t hi = (int32_t)(u >> 32);
  if (hi >= 0x40000000) {
    return INT32_MAX;
  }
  if (hi < -0x40000000) {
    return INT32_MIN;
  }
  return (fm_q31)(((uint32_t)hi << 1) | ((uint32_t)u >> 31));
}

/* round(s / 2^31), saturated: it fits when bits 62..95 are all equal */
FX_INLINE fm_q31 out31w(Sum96 s) {
  sum96_add(&s, 0x40000000);
  uint32_t top = (uint32_t)(s.lo >> 62);
  if (s.hi == 0 && top == 0U) {
    return (fm_q31)(uint32_t)(s.lo >> 31);
  }
  if (s.hi == -1 && top == 3U) {
    return (fm_q31)(uint32_t)(s.lo >> 31);
  }
  return s.hi < 0 ? INT32_MIN : INT32_MAX;
}

/* the same over 2^sh more: sh <= 16 (q15), 32 (q31) */
FX_INLINE fm_q15 out15s(int32_t a, uint32_t sh) {
  return sh == 0U ? out15(a) : fm_sat_q15(((a >> (14U + sh)) + 1) >> 1);
}

FX_INLINE fm_q15 out15ws(int64_t a, uint32_t sh) {
  if (sh == 0U) {
    return out15w(a);
  }
  int64_t r = ((a >> (14U + sh)) + 1) >> 1;
  return r > 32767 ? 32767 : (r < -32768 ? -32768 : (fm_q15)r);
}

FX_INLINE fm_q31 out31s(int64_t a, uint32_t sh) {
  if (sh == 0U) {
    return out31(a);
  }
  int64_t r = ((a >> (30U + sh)) + 1) >> 1;
  return r > INT32_MAX ? INT32_MAX : (r < INT32_MIN ? INT32_MIN : (fm_q31)r);
}

/* round(a / 2^rs) (ties up), or a 2^-rs for rs <= 0: the caller keeps it in
 * range */
FX_INLINE int32_t outr(int64_t a, int32_t rs) {
  if (rs <= 0) {
    return (int32_t)(uint32_t)((uint64_t)a << (uint32_t)-rs);
  }
  return (int32_t)(((a >> (uint32_t)(rs - 1)) + 1) >> 1);
}

/* ---- kernels: output i of m sums h[k hs] b[i + t - 1 - k] (up: b[i + k])
 * into y[i ys], or outr(sum, rs) into yr; nl / nt edge samples are 0 ---- */

#define FIR_H(n) h[up ? i + (n) * hs : i - (n) * hs]
#define FIR_STEP(n) (i = up ? i + (n) * hs : i - (n) * hs)
/* load q of the group: g - 1 + t of them, the first nl and last nt zero */
#define FIR_LD(q)                                                              \
  ((q) < nl || (nt != 0U && (q) + nt >= t + G - 1U) ? 0 : b[(q) - nl])

/* q15, sum |h| < 2: four outputs per pass, samples rotating in registers */
#define MAC4(c, s0, s1, s2, s3)                                                \
  do {                                                                         \
    a0 += (c) * (s0);                                                          \
    a1 += (c) * (s1);                                                          \
    a2 += (c) * (s2);                                                          \
    a3 += (c) * (s3);                                                          \
  } while (0)

#define G 4U
FX_INLINE void k15(const fm_q15 *h, uint32_t hs, bool up, uint32_t t,
                   const fm_q15 *b, fm_q15 *y, uint32_t ys, uint32_t m,
                   uint32_t nl, uint32_t nt, uint32_t sh, int32_t *yr,
                   int32_t rs) {
  const uint32_t top = up ? 0U : (t - 1U) * hs;
  for (; m >= 4U; m -= 4U) {
    int32_t a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    int32_t x0 = FIR_LD(0U), x1 = FIR_LD(1U), x2 = FIR_LD(2U), x3;
    const fm_q15 *bp = b + 3U - nl;
    uint32_t i = top;
    const uint32_t kr = nt == 0U ? t : (t > nt ? t - nt : 0U);
    uint32_t k = kr;
    for (; k >= 4U; k -= 4U) {
      x3 = *bp++;
      MAC4(FIR_H(0U), x0, x1, x2, x3);
      x0 = *bp++;
      MAC4(FIR_H(1U), x1, x2, x3, x0);
      x1 = *bp++;
      MAC4(FIR_H(2U), x2, x3, x0, x1);
      x2 = *bp++;
      MAC4(FIR_H(3U), x3, x0, x1, x2);
      FIR_STEP(4U);
    }
    for (; k != 0U; k--) {
      x3 = *bp++;
      MAC4(FIR_H(0U), x0, x1, x2, x3);
      FIR_STEP(1U);
      x0 = x1;
      x1 = x2;
      x2 = x3;
    }
    for (k = t - kr; k != 0U; k--) {
      MAC4(FIR_H(0U), x0, x1, x2, 0);
      FIR_STEP(1U);
      x0 = x1;
      x1 = x2;
      x2 = 0;
    }
    if (yr != NULL) {
      yr[0] = outr(a0, rs);
      yr[ys] = outr(a1, rs);
      yr[2U * ys] = outr(a2, rs);
      yr[3U * ys] = outr(a3, rs);
      yr += 4U * ys;
    } else {
      y[0] = out15s(a0, sh);
      y[ys] = out15s(a1, sh);
      y[2U * ys] = out15s(a2, sh);
      y[3U * ys] = out15s(a3, sh);
      y += 4U * ys;
    }
    b += 4;
  }
  for (; m != 0U; m--) {
    int32_t a = 0;
    uint32_t i = top;
    for (uint32_t k = 0U; k < t; k++) {
      a += FIR_H(0U) * b[k];
      FIR_STEP(1U);
    }
    if (yr != NULL) {
      *yr = outr(a, rs);
      yr += ys;
    } else {
      *y = out15s(a, sh);
      y += ys;
    }
    b++;
  }
}
#undef G

/* q15, any gain, contiguous windows: 64-bit sums, two outputs per pass */
#define G 2U
FX_INLINE void k15w2(const fm_q15 *h, uint32_t hs, bool up, uint32_t t,
                     const fm_q15 *b, fm_q15 *y, uint32_t ys, uint32_t m,
                     uint32_t nl, uint32_t nt, uint32_t sh, int32_t *yr,
                     int32_t rs) {
  const uint32_t top = up ? 0U : (t - 1U) * hs;
  for (; m >= 2U; m -= 2U) {
    int64_t a0 = 0, a1 = 0;
    int32_t x0 = FIR_LD(0U), x1;
    const fm_q15 *bp = b + 1U - nl;
    uint32_t i = top;
    const uint32_t kr = nt == 0U ? t : (t > nt ? t - nt : 0U);
    uint32_t k = kr;
    for (; k >= 2U; k -= 2U) {
      int32_t c = FIR_H(0U);
      x1 = *bp++;
      a0 = fx_smlal(a0, c, x0);
      a1 = fx_smlal(a1, c, x1);
      c = FIR_H(1U);
      x0 = *bp++;
      a0 = fx_smlal(a0, c, x1);
      a1 = fx_smlal(a1, c, x0);
      FIR_STEP(2U);
    }
    if (k != 0U) {
      x1 = *bp;
      a0 = fx_smlal(a0, FIR_H(0U), x0);
      a1 = fx_smlal(a1, FIR_H(0U), x1);
      FIR_STEP(1U);
      x0 = x1;
    }
    if (kr != t) {
      a0 = fx_smlal(a0, FIR_H(0U), x0);
    }
    if (yr != NULL) {
      yr[0] = outr(a0, rs);
      yr[ys] = outr(a1, rs);
      yr += 2U * ys;
    } else {
      y[0] = out15ws(a0, sh);
      y[ys] = out15ws(a1, sh);
      y += 2U * ys;
    }
    b += 2;
  }
  if (m != 0U) {
    int64_t a = 0;
    uint32_t i = top;
    for (uint32_t k = 0U; k < t; k++) {
      a += FIR_H(0U) * b[k];
      FIR_STEP(1U);
    }
    if (yr != NULL) {
      *yr = outr(a, rs);
    } else {
      *y = out15ws(a, sh);
    }
  }
}
#undef G

/* q31, sum |h| < 2: three outputs per pass (four accumulators spill) */
#define SMLAL3(c, s0, s1, s2)                                                  \
  do {                                                                         \
    a0 = fx_smlal(a0, (c), (s0));                                              \
    a1 = fx_smlal(a1, (c), (s1));                                              \
    a2 = fx_smlal(a2, (c), (s2));                                              \
  } while (0)

#define G 3U
FX_INLINE void k31(const fm_q31 *h, uint32_t hs, bool up, uint32_t t,
                   const fm_q31 *b, fm_q31 *y, uint32_t ys, uint32_t m,
                   uint32_t nl, uint32_t nt, uint32_t sh, int32_t *yr,
                   int32_t rs) {
  const uint32_t top = up ? 0U : (t - 1U) * hs;
  for (; m >= 3U; m -= 3U) {
    int64_t a0 = 0, a1 = 0, a2 = 0;
    int32_t x0 = FIR_LD(0U), x1 = FIR_LD(1U), x2;
    const fm_q31 *bp = b + 2U - nl;
    uint32_t i = top;
    const uint32_t kr = nt == 0U ? t : (t > nt ? t - nt : 0U);
    uint32_t k = kr;
    for (; k >= 3U; k -= 3U) {
      x2 = *bp++;
      SMLAL3(FIR_H(0U), x0, x1, x2);
      x0 = *bp++;
      SMLAL3(FIR_H(1U), x1, x2, x0);
      x1 = *bp++;
      SMLAL3(FIR_H(2U), x2, x0, x1);
      FIR_STEP(3U);
    }
    for (; k != 0U; k--) {
      x2 = *bp++;
      SMLAL3(FIR_H(0U), x0, x1, x2);
      FIR_STEP(1U);
      x0 = x1;
      x1 = x2;
    }
    for (k = t - kr; k != 0U; k--) {
      SMLAL3(FIR_H(0U), x0, x1, 0);
      FIR_STEP(1U);
      x0 = x1;
      x1 = 0;
    }
    if (yr != NULL) {
      yr[0] = outr(a0, rs);
      yr[ys] = outr(a1, rs);
      yr[2U * ys] = outr(a2, rs);
      yr += 3U * ys;
    } else {
      y[0] = out31s(a0, sh);
      y[ys] = out31s(a1, sh);
      y[2U * ys] = out31s(a2, sh);
      y += 3U * ys;
    }
    b += 3;
  }
  for (; m != 0U; m--) {
    int64_t a = 0;
    uint32_t i = top;
    for (uint32_t k = 0U; k < t; k++) {
      a = fx_smlal(a, FIR_H(0U), b[k]);
      FIR_STEP(1U);
    }
    if (yr != NULL) {
      *yr = outr(a, rs);
      yr += ys;
    } else {
      *y = out31s(a, sh);
      y += ys;
    }
    b++;
  }
}
#undef G

/* ---- decimators: output i of m over b[i ws ..], taps h[k hs], into y[i ys];
 * plain static like the state helpers, inlined where it pays ---- */

#define FIR_LOCAL static __attribute__((unused))

/* q15, sum |h| < 2: four outputs per pass */
FIR_LOCAL void k15d(const fm_q15 *h, uint32_t hs, uint32_t t, const fm_q15 *b,
                    uint32_t ws, fm_q15 *y, uint32_t ys, uint32_t m) {
  for (; m >= 4U; m -= 4U) {
    int32_t a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    const fm_q15 *p0 = b, *p1 = b + ws, *p2 = b + 2U * ws, *p3 = b + 3U * ws;
    for (uint32_t k = t; k != 0U; k--) {
      int32_t c = h[(k - 1U) * hs];
      a0 += c * *p0++;
      a1 += c * *p1++;
      a2 += c * *p2++;
      a3 += c * *p3++;
    }
    y[0] = out15(a0);
    y[ys] = out15(a1);
    y[2U * ys] = out15(a2);
    y[3U * ys] = out15(a3);
    y += 4U * ys;
    b += 4U * ws;
  }
  for (; m != 0U; m--) {
    int32_t a = 0;
    for (uint32_t k = 0U; k < t; k++) {
      a += h[(t - 1U - k) * hs] * b[k];
    }
    *y = out15(a);
    y += ys;
    b += ws;
  }
}

/* q15, any gain: 64-bit sums */
FIR_LOCAL void k15w(const fm_q15 *h, uint32_t hs, uint32_t t, const fm_q15 *b,
                    uint32_t ws, fm_q15 *y, uint32_t ys, uint32_t m) {
  for (; m != 0U; m--) {
    int64_t a = 0;
    uint32_t i = (t - 1U) * hs;
    for (uint32_t k = 0U; k < t; k++) {
      a += h[i] * b[k];
      i -= hs;
    }
    *y = out15w(a);
    y += ys;
    b += ws;
  }
}

/* q31, sum |h| < 2: two outputs per pass */
FIR_LOCAL void k31d(const fm_q31 *h, uint32_t hs, uint32_t t, const fm_q31 *b,
                    uint32_t ws, fm_q31 *y, uint32_t ys, uint32_t m) {
  for (; m >= 2U; m -= 2U) {
    int64_t a0 = 0, a1 = 0;
    const fm_q31 *p0 = b, *p1 = b + ws;
    for (uint32_t k = t; k != 0U; k--) {
      int32_t c = h[(k - 1U) * hs];
      a0 = fx_smlal(a0, c, *p0++);
      a1 = fx_smlal(a1, c, *p1++);
    }
    y[0] = out31(a0);
    y[ys] = out31(a1);
    y += 2U * ys;
    b += 2U * ws;
  }
  if (m != 0U) {
    int64_t a = 0;
    for (uint32_t k = 0U; k < t; k++) {
      a = fx_smlal(a, h[(t - 1U - k) * hs], b[k]);
    }
    *y = out31(a);
  }
}

/* q31, any gain: 96-bit sums */
FIR_LOCAL void k31w(const fm_q31 *h, uint32_t hs, uint32_t t, const fm_q31 *b,
                    uint32_t ws, fm_q31 *y, uint32_t ys, uint32_t m) {
  for (; m != 0U; m--) {
    Sum96 s = {0U, 0};
    uint32_t i = (t - 1U) * hs;
    for (uint32_t k = 0U; k < t; k++) {
      sum96_add(&s, fx_smull(h[i], b[k]));
      i -= hs;
    }
    *y = out31w(s);
    y += ys;
    b += ws;
  }
}

/* ---- state: the history, then room for new samples ---- */

/* forward copies, so they may move the history down over itself */
FIR_LOCAL void move15(fm_q15 *d, const fm_q15 *s, uint32_t n) {
  FX_OPAQUE(d);
  for (uint32_t i = 0U; i < n; i++) {
    d[i] = s[i];
  }
}

FIR_LOCAL void move31(fm_q31 *d, const fm_q31 *s, uint32_t n) {
  FX_OPAQUE(d);
  for (uint32_t i = 0U; i < n; i++) {
    d[i] = s[i];
  }
}

/* room for up to n new samples, stored; returns how many */
FIR_LOCAL uint32_t take15(FD_FirQ15 *f, const fm_q15 *x, uint32_t n) {
  if (f->w == f->len) {
    move15(f->state, f->state + f->w - (f->ntaps - 1U), f->ntaps - 1U);
    f->w = f->ntaps - 1U;
  }
  uint32_t m = f->len - f->w < n ? f->len - f->w : n;
  move15(f->state + f->w, x, m);
  return m;
}

FIR_LOCAL uint32_t take31(FD_FirQ31 *f, const fm_q31 *x, uint32_t n) {
  if (f->w == f->len) {
    move31(f->state, f->state + f->w - (f->ntaps - 1U), f->ntaps - 1U);
    f->w = f->ntaps - 1U;
  }
  uint32_t m = f->len - f->w < n ? f->len - f->w : n;
  move31(f->state + f->w, x, m);
  return m;
}

/* ---- design ---- */

#define PI_F 3.14159265358979f

/* sample k of an n-point window (float, for the design helpers) */
FIR_LOCAL float win_value(FD_Window win, uint32_t k, uint32_t n) {
  static const float a[][5] = {
      {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},                     /* rectangular */
      {0.5f, 0.5f, 0.0f, 0.0f, 0.0f},                     /* Hann */
      {0.54f, 0.46f, 0.0f, 0.0f, 0.0f},                   /* Hamming */
      {0.42f, 0.5f, 0.08f, 0.0f, 0.0f},                   /* Blackman */
      {0.35875f, 0.48829f, 0.14128f, 0.01168f, 0.0f},     /* Blackman-Harris */
      {0.355768f, 0.487396f, 0.144232f, 0.012604f, 0.0f}, /* Nuttall */
      {0.21557895f, 0.41663158f, 0.277263158f, 0.083578947f, 0.006947368f},
  };
  if (n == 1U) {
    return 1.0f;
  }
  if (win == FD_WIN_BARTLETT) {
    float r = (2.0f * (float)k - (float)(n - 1U)) / (float)(n - 1U);
    return 1.0f - (r < 0.0f ? -r : r);
  }
  float t = 2.0f * PI_F * (float)k / (float)(n - 1U);
  const float *c = a[win];
  return c[0] - c[1] * fm_cosf(t) + c[2] * fm_cosf(2.0f * t) -
         c[3] * fm_cosf(3.0f * t) + c[4] * fm_cosf(4.0f * t);
}

#endif /* FD_FIR_CORE_H */
