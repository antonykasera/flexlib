/**
 * @file fd_corr.c
 * @brief Convolution and correlation of q15 / q31 blocks
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_corr.h"
#include "fd_fft.h"
#include "fd_fir_core.h"
#include "fd_vec.h"
#include "fm_div.h"

/* Output i windows a from s = s0 + i: a[s + p] times c(p) for the p in
 * [0, nb) that fall inside a, with c(p) = b[nb - 1 - p] (convolution, rev)
 * or b[p] (correlation). Runs of whole windows go to the FIR kernels; at the
 * edges each group of outputs runs the kernel over the union of their p, the
 * samples outside a read as zero. */

#define C(p) (rev ? b[nb - 1 - (p)] : b[p])

FX_INLINE int32_t lo_of(int32_t s) { return s < 0 ? -s : 0; }

FX_INLINE int32_t hi_of(int32_t s, int32_t na, int32_t nb) {
  return na - 1 - s < nb - 1 ? na - 1 - s : nb - 1;
}

/* Every partial sum is at most max |a| sum |b|, and max |b| sum |a|: under
 * the bound the narrow kernels are exact. The scan stops once the answer is
 * known: both products over (wide), or one input done with the largest
 * possible max of the other times its sum under (narrow). b, usually the
 * filter or template, is read to the end before the rest of a. */
typedef struct {
  uint64_t s;
  uint32_t m, i;
} Stat;

#define SCAN_CHUNK 16U

#define STAT_STEP(x, n, st, absv)                                              \
  do {                                                                         \
    uint32_t e_ = (st).i + SCAN_CHUNK < (n) ? (st).i + SCAN_CHUNK : (n);       \
    for (; (st).i < e_; (st).i++) {                                            \
      uint32_t u_ = absv((x)[(st).i]);                                         \
      (st).s += u_;                                                            \
      (st).m = u_ > (st).m ? u_ : (st).m;                                      \
    }                                                                          \
  } while (0)

/* over(m, s): m s past the bound; top: the largest |x| */
#define WIDE_SCAN(a, na, b, nb, absv, over, top)                               \
  do {                                                                         \
    Stat sa = {0U, 0U, 0U}, sb = {0U, 0U, 0U};                                 \
    if ((a) == (b) && (na) == (nb)) {                                          \
      while (sa.i != (na)) {                                                   \
        STAT_STEP(a, na, sa, absv);                                            \
        if (over(sa.m, sa.s)) {                                                \
          return true;                                                         \
        }                                                                      \
      }                                                                        \
      return false;                                                            \
    }                                                                          \
    STAT_STEP(a, na, sa, absv);                                                \
    do {                                                                       \
      STAT_STEP(b, nb, sb, absv);                                              \
      if (over(sa.m, sb.s) && over(sb.m, sa.s)) {                              \
        return true;                                                           \
      }                                                                        \
    } while (sb.i != nb);                                                      \
    if (!over(top, sb.s)) {                                                    \
      return false;                                                            \
    }                                                                          \
    while (sa.i != na) {                                                       \
      STAT_STEP(a, na, sa, absv);                                              \
      if (over(sa.m, sb.s) && over(sb.m, sa.s)) {                              \
        return true;                                                           \
      }                                                                        \
    }                                                                          \
    return over(sa.m, sb.s) && over(sb.m, sa.s);                               \
  } while (0)

FX_INLINE uint32_t abs15(int32_t v) { return (uint32_t)(v < 0 ? -v : v); }
FX_INLINE uint32_t abs31(int32_t v) {
  return v < 0 ? 0U - (uint32_t)v : (uint32_t)v;
}

/* 2^31 - 2^15 and 2^63 - 2^31: the rounding still fits */
FX_INLINE bool over15(uint32_t m, uint64_t s) { return m * s > 0x7FFF8000U; }

FX_INLINE bool over31(uint32_t m, uint64_t s) {
  const FD_U128 lim = fd_u128(0U, 0x7FFFFFFF80000000U);
  return !fd_u128_ge(lim, fd_u128_mul64(m, s));
}

static bool wide15(const fm_q15 *a, uint32_t na, const fm_q15 *b, uint32_t nb) {
  WIDE_SCAN(a, na, b, nb, abs15, over15, 32768U);
}

static bool wide31(const fm_q31 *a, uint32_t na, const fm_q31 *b, uint32_t nb) {
  WIDE_SCAN(a, na, b, nb, abs31, over31, 0x80000000U);
}

/* one output: window s over all its p */
FX_INLINE int64_t one15(const fm_q15 *a, int32_t na, const fm_q15 *b,
                        int32_t nb, bool rev, int32_t s) {
  int64_t acc = 0;
  for (int32_t p = lo_of(s), e = hi_of(s, na, nb); p <= e; p++) {
    acc += C(p) * a[s + p];
  }
  return acc;
}

/* a group of g outputs from s: the kernel's p range, h, the first real
 * sample and how many zeros precede it and follow the data */
typedef struct {
  int32_t lo, hi;
  uint32_t nl, nt;
  int32_t first;
} Edge;

FX_INLINE Edge edge(int32_t s, uint32_t g, int32_t na, int32_t nb) {
  Edge e;
  e.lo = lo_of(s + (int32_t)g - 1);
  e.hi = hi_of(s, na, nb);
  int32_t v = s + e.lo, last = s + (int32_t)g - 1 + e.hi;
  e.nl = v < 0 ? (uint32_t)-v : 0U;
  e.nt = last > na - 1 ? (uint32_t)(last - (na - 1)) : 0U;
  e.first = v < 0 ? 0 : v;
  return e;
}

/* whole groups of interior outputs from s (none before a, none short) */
FX_INLINE uint32_t interior(int32_t s, int32_t na, int32_t nb, uint32_t n,
                            uint32_t g) {
  if (s < 0 || s > na - nb) {
    return 0U;
  }
  uint32_t m = (uint32_t)(na - nb - s) + 1U;
  m = m < n ? m : n;
  return g == 3U ? m - m % 3U : m & ~(g - 1U);
}

/* Outputs to y, or with yr to yr as outr(sum, rs). A last group short of g
 * steps back over outputs already written: they come out the same. */
FX_INLINE void xc15(const fm_q15 *a, int32_t na, const fm_q15 *b, int32_t nb,
                    bool rev, int32_t s, uint32_t n, uint32_t sh, bool wide,
                    fm_q15 *y, int32_t *yr, int32_t rs) {
  const uint32_t g = wide ? 2U : 4U;
  uint32_t done = 0U;
  while (n != 0U) {
    uint32_t m = interior(s, na, nb, n, g);
    if (m != 0U) {
      if (wide) {
        k15w2(b, 1U, !rev, (uint32_t)nb, a + s, y, 1U, m, 0U, 0U, sh, yr, rs);
      } else {
        k15(b, 1U, !rev, (uint32_t)nb, a + s, y, 1U, m, 0U, 0U, sh, yr, rs);
      }
    } else if (n >= g || done >= g - n) {
      if (n < g) {
        uint32_t back = g - n;
        s -= (int32_t)back;
        done -= back;
        n = g;
        if (yr != NULL) {
          yr -= back;
        } else {
          y -= back;
        }
      }
      m = g;
      Edge e = edge(s, g, na, nb);
      if (e.hi < e.lo) {
        if (yr != NULL) {
          yr[0] = yr[1] = 0;
          if (!wide) {
            yr[2] = yr[3] = 0;
          }
        } else {
          y[0] = y[1] = 0;
          if (!wide) {
            y[2] = y[3] = 0;
          }
        }
      } else {
        const fm_q15 *h = rev ? b + (nb - 1 - e.hi) : b + e.lo;
        const uint32_t t = (uint32_t)(e.hi - e.lo + 1);
        if (wide) {
          k15w2(h, 1U, !rev, t, a + e.first, y, 1U, 2U, e.nl, e.nt, sh, yr, rs);
        } else {
          k15(h, 1U, !rev, t, a + e.first, y, 1U, 4U, e.nl, e.nt, sh, yr, rs);
        }
      }
    } else {
      m = 1U;
      if (yr != NULL) {
        *yr = outr(one15(a, na, b, nb, rev, s), rs);
      } else {
        *y = out15ws(one15(a, na, b, nb, rev, s), sh);
      }
    }
    s += (int32_t)m;
    if (yr != NULL) {
      yr += m;
    } else {
      y += m;
    }
    n -= m;
    done += m;
  }
}

/* round(s / 2^(31 + sh)), saturated */
FX_INLINE fm_q31 out31ws(Sum96 s, uint32_t sh) {
  if (sh == 0U) {
    return out31w(s);
  }
  sum96_add(&s, (int64_t)1 << (30U + sh));
  int64_t t = (int64_t)(((uint64_t)(int64_t)s.hi << 32) | (s.lo >> 32));
  int64_t r = t >> (sh - 1U);
  return r > INT32_MAX ? INT32_MAX : (r < INT32_MIN ? INT32_MIN : (fm_q31)r);
}

/* one output in 96 bits (wide) */
FX_INLINE Sum96 one31w(const fm_q31 *a, int32_t na, const fm_q31 *b, int32_t nb,
                       bool rev, int32_t s) {
  Sum96 acc = {0U, 0};
  for (int32_t p = lo_of(s), e = hi_of(s, na, nb); p <= e; p++) {
    sum96_add(&acc, fx_smull(C(p), a[s + p]));
  }
  return acc;
}

/* and in 64 (narrow) */
FX_INLINE int64_t one31(const fm_q31 *a, int32_t na, const fm_q31 *b,
                        int32_t nb, bool rev, int32_t s) {
  int64_t acc = 0;
  for (int32_t p = lo_of(s), e = hi_of(s, na, nb); p <= e; p++) {
    acc = fx_smlal(acc, C(p), a[s + p]);
  }
  return acc;
}

/* round(s / 2^rs) to int32 as outr (rs >= 33 when s needs more than 64 bits) */
FX_INLINE int32_t outr96(Sum96 s, int32_t rs) {
  uint32_t top = (uint32_t)(s.lo >> 63);
  if ((s.hi == 0 && top == 0U) || (s.hi == -1 && top == 1U)) {
    return outr((int64_t)s.lo, rs);
  }
  sum96_add(&s, (int64_t)1 << (uint32_t)(rs - 1));
  int64_t t = (int64_t)(((uint64_t)(int64_t)s.hi << 32) | (s.lo >> 32));
  return (int32_t)(t >> (uint32_t)(rs - 32));
}

FX_INLINE void xc31(const fm_q31 *a, int32_t na, const fm_q31 *b, int32_t nb,
                    bool rev, int32_t s, uint32_t n, uint32_t sh, bool wide,
                    fm_q31 *y, int32_t *yr, int32_t rs) {
  if (wide) {
    for (; n != 0U; n--) {
      Sum96 v = one31w(a, na, b, nb, rev, s);
      if (yr != NULL) {
        *yr++ = outr96(v, rs);
      } else {
        *y++ = out31ws(v, sh);
      }
      s++;
    }
    return;
  }
  uint32_t done = 0U;
  while (n != 0U) {
    uint32_t m = interior(s, na, nb, n, 3U);
    if (m != 0U) {
      k31(b, 1U, !rev, (uint32_t)nb, a + s, y, 1U, m, 0U, 0U, sh, yr, rs);
    } else if (n >= 3U || done >= 3U - n) {
      if (n < 3U) {
        uint32_t back = 3U - n;
        s -= (int32_t)back;
        done -= back;
        n = 3U;
        if (yr != NULL) {
          yr -= back;
        } else {
          y -= back;
        }
      }
      m = 3U;
      Edge e = edge(s, 3U, na, nb);
      if (e.hi < e.lo) {
        int32_t *z = yr != NULL ? yr : y;
        z[0] = z[1] = z[2] = 0;
      } else {
        k31(rev ? b + (nb - 1 - e.hi) : b + e.lo, 1U, !rev,
            (uint32_t)(e.hi - e.lo + 1), a + e.first, y, 1U, 3U, e.nl, e.nt, sh,
            yr, rs);
      }
    } else {
      m = 1U;
      if (yr != NULL) {
        *yr = outr(one31(a, na, b, nb, rev, s), rs);
      } else {
        *y = out31s(one31(a, na, b, nb, rev, s), sh);
      }
    }
    s += (int32_t)m;
    if (yr != NULL) {
      yr += m;
    } else {
      y += m;
    }
    n -= m;
    done += m;
  }
}

/* ---- the public forms: one body each for convolution and correlation ---- */

void fd_conv_part_q15(const fm_q15 *a, uint32_t na, const fm_q15 *b,
                      uint32_t nb, uint32_t shift, uint32_t first, uint32_t n,
                      fm_q15 *y) {
  FX_ASSERT(na >= 1U && nb >= 1U && (na | nb) < 0x80000000U && shift <= 16U);
  FX_ASSERT(first <= na + nb - 1U && n <= na + nb - 1U - first);
  xc15(a, (int32_t)na, b, (int32_t)nb, true, (int32_t)first - (int32_t)nb + 1,
       n, shift, wide15(a, na, b, nb), y, NULL, 0);
}

void fd_conv_part_q31(const fm_q31 *a, uint32_t na, const fm_q31 *b,
                      uint32_t nb, uint32_t shift, uint32_t first, uint32_t n,
                      fm_q31 *y) {
  FX_ASSERT(na >= 1U && nb >= 1U && (na | nb) < 0x80000000U && shift <= 32U);
  FX_ASSERT(first <= na + nb - 1U && n <= na + nb - 1U - first);
  xc31(a, (int32_t)na, b, (int32_t)nb, true, (int32_t)first - (int32_t)nb + 1,
       n, shift, wide31(a, na, b, nb), y, NULL, 0);
}

void fd_conv_q15(const fm_q15 *a, uint32_t na, const fm_q15 *b, uint32_t nb,
                 uint32_t shift, fm_q15 *y) {
  fd_conv_part_q15(a, na, b, nb, shift, 0U, na + nb - 1U, y);
}

void fd_conv_q31(const fm_q31 *a, uint32_t na, const fm_q31 *b, uint32_t nb,
                 uint32_t shift, fm_q31 *y) {
  fd_conv_part_q31(a, na, b, nb, shift, 0U, na + nb - 1U, y);
}

void fd_corr_q15(const fm_q15 *a, uint32_t na, const fm_q15 *b, uint32_t nb,
                 int32_t lag0, uint32_t nlags, uint32_t shift, fm_q15 *y) {
  FX_ASSERT(na >= 1U && nb >= 1U && (na | nb) < 0x80000000U && shift <= 16U);
  xc15(a, (int32_t)na, b, (int32_t)nb, false, lag0, nlags, shift,
       wide15(a, na, b, nb), y, NULL, 0);
}

void fd_corr_q31(const fm_q31 *a, uint32_t na, const fm_q31 *b, uint32_t nb,
                 int32_t lag0, uint32_t nlags, uint32_t shift, fm_q31 *y) {
  FX_ASSERT(na >= 1U && nb >= 1U && (na | nb) < 0x80000000U && shift <= 32U);
  xc31(a, (int32_t)na, b, (int32_t)nb, false, lag0, nlags, shift,
       wide31(a, na, b, nb), y, NULL, 0);
}

/* ---- autocorrelation: block floating, r[0] in [2^30, 2^31) ---- */

FX_INLINE uint32_t bits64(uint64_t v) {
  uint32_t h = (uint32_t)(v >> 32);
  return h != 0U ? 64U - fx_clz(h) : 32U - fx_clz((uint32_t)v);
}

FX_INLINE uint32_t bits96(Sum96 v) {
  return v.hi != 0 ? 96U - fx_clz((uint32_t)v.hi) : bits64(v.lo);
}

/* r0 > 0 to 2^rs: rs = bits - 31, one more when rounding carries to 2^31 */
static int32_t norm64(uint64_t r0) {
  int32_t rs = (int32_t)bits64(r0) - 31;
  if (rs > 0 &&
      bits64(r0 + (1ULL << (uint32_t)(rs - 1))) > (uint32_t)rs + 31U) {
    rs++;
  }
  return rs;
}

static int32_t norm96(Sum96 r0) {
  int32_t rs = (int32_t)bits96(r0) - 31;
  if (rs > 0) {
    Sum96 t = r0;
    sum96_add(&t, (int64_t)1 << (uint32_t)(rs - 1));
    rs += bits96(t) > (uint32_t)rs + 31U ? 1 : 0;
  }
  return rs;
}

int32_t fd_autocorr_q15(const fm_q15 *x, uint32_t n, fm_q31 *r,
                        uint32_t nlags) {
  FX_ASSERT(n >= 1U && n < 0x80000000U && nlags >= 1U);
  const int32_t m = (int32_t)n;
  if (!wide15(x, n, x, n)) {
    /* every sum fits int32: raw first, then up to r[0]'s top bit */
    xc15(x, m, x, m, false, 0, nlags, 0U, false, NULL, r, 0);
    if (r[0] == 0) {
      return 0;
    }
    int32_t rs = norm64((uint64_t)r[0]);
    for (uint32_t k = 0U; k < nlags; k++) {
      r[k] = outr(r[k], rs);
    }
    return rs + 1;
  }
  int32_t rs = norm64((uint64_t)one15(x, m, x, m, false, 0));
  xc15(x, m, x, m, false, 0, nlags, 0U, true, NULL, r, rs);
  return rs + 1;
}

int32_t fd_autocorr_q31(const fm_q31 *x, uint32_t n, fm_q31 *r,
                        uint32_t nlags) {
  FX_ASSERT(n >= 1U && n < 0x80000000U && nlags >= 1U);
  const int32_t m = (int32_t)n;
  const bool wide = wide31(x, n, x, n);
  int32_t rs;
  if (wide) {
    rs = norm96(one31w(x, m, x, m, false, 0));
  } else {
    int64_t r0 = one31(x, m, x, m, false, 0);
    if (r0 == 0) {
      for (uint32_t k = 0U; k < nlags; k++) {
        r[k] = 0;
      }
      return 0;
    }
    rs = norm64((uint64_t)r0);
  }
  xc31(x, m, x, m, false, 0, nlags, 0U, wide, NULL, r, rs);
  return rs - 31;
}

/* ---- correlation through q15 FFTs ---- */

/* round(v / 2^16), saturated: v in (-2^31, 2^31] */
FX_INLINE fm_q15 rnd16(int64_t v) {
  int64_t r = (v + 0x8000) >> 16;
  return r > 32767 ? 32767 : (fm_q15)r;
}

/* a = A conj(B) bin by bin in fd_rfft's layout, each an exact sum of two
 * products rounded once */
static void cmulc15(fm_q15 *a, const fm_q15 *b, uint32_t n) {
  a[0] = rnd16(a[0] * b[0]);
  a[1] = rnd16(a[1] * b[1]);
  for (uint32_t k = 2U; k < n; k += 2U) {
    int32_t ar = a[k], ai = a[k + 1U], br = b[k], bi = b[k + 1U];
    a[k] = rnd16((int64_t)(ar * br) + ai * bi);
    a[k + 1U] = rnd16((int64_t)(ai * br) - ar * bi);
  }
}

int32_t fd_corr_fft_q15(fm_q15 *a, fm_q15 *b, uint32_t n) {
  int32_t ea = fd_rfft_q15(a, n), eb = fd_rfft_q15(b, n);
  cmulc15(a, b, n);
  return fd_irfft_q15(a, n) + ea + eb + 1;
}

/* ---- peak ---- */

/* round(num 2^16 / den) (ties up), den > 0, |num| <= den / 2 */
static int32_t frac16(int64_t num, uint64_t den) {
  uint64_t v = (uint64_t)(num < 0 ? -num : num);
  uint32_t q = 0U; /* floor(v 2^17 / den), the bit below for rounding */
  for (uint32_t i = 0U; i < 17U; i++) {
    v <<= 1;
    q <<= 1;
    if (v >= den) {
      v -= den;
      q |= 1U;
    }
  }
  return num < 0 ? -(int32_t)((q + (v != 0U ? 1U : 0U)) >> 1)
                 : (int32_t)((q + 1U) >> 1);
}

/* i in Q16.16, moved by the vertex of the parabola through i - 1, i, i + 1 */
FX_INLINE fm_q16 vertex(uint32_t i, uint32_t n, int64_t ym, int64_t y0,
                        int64_t yp) {
  int32_t d = 0;
  if (i > 0U && i + 1U < n) { /* y0 is the first maximum: ym < y0 */
    d = frac16(yp - ym, (uint64_t)(2 * (2 * y0 - ym - yp)));
  }
  return (fm_q16)(int32_t)((i << 16) + (uint32_t)d);
}

fm_q16 fd_peak_q15(const fm_q15 *y, uint32_t n, fm_q15 *peak) {
  FX_ASSERT(n >= 1U && n <= 32768U);
  uint32_t i;
  fm_q15 v = fd_max_q15(y, n, &i);
  if (peak != NULL) {
    *peak = v;
  }
  return vertex(i, n, i > 0U ? y[i - 1U] : 0, v, i + 1U < n ? y[i + 1U] : 0);
}

fm_q16 fd_peak_q31(const fm_q31 *y, uint32_t n, fm_q31 *peak) {
  FX_ASSERT(n >= 1U && n <= 32768U);
  uint32_t i;
  fm_q31 v = fd_max_q31(y, n, &i);
  if (peak != NULL) {
    *peak = v;
  }
  return vertex(i, n, i > 0U ? y[i - 1U] : 0, v, i + 1U < n ? y[i + 1U] : 0);
}

/* ---- Levinson-Durbin ---- */

/* round(s / 2^sh) for 1 <= sh <= 31, saturated to int64 */
static int64_t round96(Sum96 s, uint32_t sh) {
  sum96_add(&s, (int64_t)1 << (sh - 1U));
  int32_t hi = s.hi >> sh;
  uint64_t lo = (s.lo >> sh) | ((uint64_t)(uint32_t)s.hi << (64U - sh));
  if (hi != ((lo >> 63) != 0U ? -1 : 0)) {
    return hi < 0 ? INT64_MIN : INT64_MAX;
  }
  return (int64_t)lo;
}

/* round(v / 2^31) when it fits int32 */
FX_INLINE bool fit31(int64_t v, int32_t *out) {
  int64_t q = ((v >> 30) + 1) >> 1;
  *out = (int32_t)q;
  return q >= INT32_MIN && q <= INT32_MAX;
}

FD_Status fd_levinson_q31(const fm_q31 *r, uint32_t order, uint32_t ashift,
                          fm_q31 *a, fm_q31 *k, fm_q31 *err) {
  FX_ASSERT(order >= 1U && order < 64U && ashift <= 30U);
  if (r[0] <= 0) {
    return FD_ERR_UNSTABLE;
  }
  const uint32_t as = 31U - ashift; /* a's fraction bits */
  int32_t e = r[0];
  for (uint32_t i = 1U; i <= order; i++) {
    /* r[i] - sum a_j r[i - j], exact in a's format times r's */
    Sum96 s = {0U, 0};
    sum96_add(&s, (int64_t)((uint64_t)(int64_t)r[i] << as));
    /* |r[k]| < 2^31, |a| <= 2^31: two products fit an int64 */
    uint32_t q = 1U;
    for (; q + 1U < i; q += 2U) {
      sum96_add(
          &s, -(fx_smull(a[q - 1U], r[i - q]) + fx_smull(a[q], r[i - q - 1U])));
    }
    if (q < i) {
      sum96_add(&s, -fx_smull(a[q - 1U], r[i - q]));
    }
    int64_t acc = round96(s, as);
    if (acc >= e || acc <= -(int64_t)e) {
      return FD_ERR_UNSTABLE;
    }
    fm_q31 ki = fm_div_q31((fm_q31)acc, e);
    for (uint32_t j = 1U; 2U * j <= i; j++) {
      int64_t lo =
          (int64_t)a[j - 1U] * 2147483648LL - fx_smull(ki, a[i - j - 1U]);
      int64_t hi =
          (int64_t)a[i - j - 1U] * 2147483648LL - fx_smull(ki, a[j - 1U]);
      int32_t vlo, vhi;
      if (!fit31(lo, &vlo) || !fit31(hi, &vhi)) {
        return FD_ERR_RANGE;
      }
      a[j - 1U] = vlo;
      a[i - j - 1U] = vhi;
    }
    a[i - 1U] = ashift == 0U ? ki : outr(ki, (int32_t)ashift);
    /* |acc| <= e - 1 keeps this >= 1 */
    (void)fit31((int64_t)e * 2147483648LL - fx_smull(ki, (int32_t)acc), &e);
    if (k != NULL) {
      k[i - 1U] = ki;
    }
  }
  if (err != NULL) {
    *err = e;
  }
  return FD_OK;
}
