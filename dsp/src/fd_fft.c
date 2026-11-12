/**
 * @file fd_fft.c
 * @brief Complex FFTs: radix-4 decimation in time, block floating point
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_fft_core.h"
#include "fd_vec.h"
#include <string.h>

_Static_assert(FD_FFT_MAX >= 4U && FD_FFT_MAX <= 4096U &&
                   (FD_FFT_MAX & (FD_FFT_MAX - 1U)) == 0U,
               "FD_FFT_MAX: a power of two, 4..4096");

#if FD_FFT_MAX > 1024U
#include "fd_fft_tab.h"
#endif

#define QP FD_FFT_QP
#define TAB FD_FFT_TAB
#define NOINLINE __attribute__((noinline))

/* the struct array as its re, im values (the first member's type) */
#define FLAT(x)                                                                \
  _Generic((x),                                                                \
      fd_cq15 *: (int16_t *)(void *)(x),                                       \
      fd_cq31 *: (int32_t *)(void *)(x))

/* ---- q15: a complex value is one aligned word ---- */

FX_INLINE uint32_t ld32(const int16_t *p) {
  uint32_t w;
  memcpy(&w, p, 4U);
  return w;
}

FX_INLINE void st32(int16_t *p, uint32_t w) { memcpy(p, &w, 4U); }

FX_INLINE void swap15(int16_t *x, uint32_t a, uint32_t b) {
  uint32_t u = ld32(x + 2U * a), v = ld32(x + 2U * b);
  st32(x + 2U * a, v);
  st32(x + 2U * b, u);
}

FX_INLINE uint32_t pair_bits(uint32_t w) { return w ^ (w << 1); }

/* even i < n/2 and r = rev(i): (i + 1, r + n/2) always, two more if i < r;
 * returns the scan's OR of every word (the fixed points i = r included) */
FX_INLINE uint32_t rev15(int16_t *x, uint32_t n) {
  const uint32_t h = n >> 1;
  const uint32_t s = fx_clz(n) + 1U;
  uint32_t acc = 0U;
  for (uint32_t i = 0U; i < h; i += 2U) {
    uint32_t r = fx_rbit(i) >> s;
    uint32_t u = ld32(x + 2U * (i + 1U)), v = ld32(x + 2U * (r + h));
    st32(x + 2U * (i + 1U), v);
    st32(x + 2U * (r + h), u);
    acc |= pair_bits(u) | pair_bits(v);
    if (i <= r) {
      uint32_t a = ld32(x + 2U * i), b = ld32(x + 2U * (i + h + 1U));
      acc |= pair_bits(a) | pair_bits(b);
      if (i < r) {
        uint32_t c = ld32(x + 2U * r), d = ld32(x + 2U * (r + h + 1U));
        acc |= pair_bits(c) | pair_bits(d);
        st32(x + 2U * i, c);
        st32(x + 2U * r, a);
        st32(x + 2U * (i + h + 1U), d);
        st32(x + 2U * (r + h + 1U), b);
      }
    }
  }
  return acc;
}

FX_INLINE uint32_t bits15(uint32_t acc) {
  return fd_fft_bits((acc >> 16) | (acc & 0xFFFFU));
}

/* n >= 4 */
uint32_t fd_fft15_rev(int16_t *x, uint32_t n) { return bits15(rev15(x, n)); }

uint32_t fd_fft15_scan(const int16_t *x, uint32_t n) {
  uint32_t acc = 0U;
  for (uint32_t k = n >> 3; k != 0U; k--) {
    uint32_t a = pair_bits(ld32(x)) | pair_bits(ld32(x + 2));
    uint32_t b = pair_bits(ld32(x + 4)) | pair_bits(ld32(x + 6));
    uint32_t c = pair_bits(ld32(x + 8)) | pair_bits(ld32(x + 10));
    uint32_t d = pair_bits(ld32(x + 12)) | pair_bits(ld32(x + 14));
    acc |= a | b | c | d;
    x += 16;
  }
  for (uint32_t k = n & 7U; k != 0U; k--) {
    acc |= pair_bits(ld32(x));
    x += 2;
  }
  /* bit 16 mixes re's sign into im's bit 0, which never sets b */
  return bits15(acc);
}

void fd_fft15_flip(int16_t *x, uint32_t n) {
  for (uint32_t a = 1U, b = n - 1U; a < b; a++, b--) {
    swap15(x, a, b);
  }
}

/* rounding constants are 2^(k-1) - 1: nearest, ties down */
#define OUT15R(v, s) (int16_t)((v) >> (s))
#define OUT15L(v, s) (int16_t)(int32_t)((uint32_t)(v) << (s))

void fd_fft15_t2r(int16_t *x, uint32_t n, int32_t sh) {
  const uint32_t s = (uint32_t)sh;
  const int32_t rnd = (1 << (s - 1U)) - 1;
  for (uint32_t k = n >> 1; k != 0U; k--) {
    int32_t ar = x[0] + rnd, ai = x[1] + rnd, br = x[2], bi = x[3];
    x[0] = OUT15R(ar + br, s);
    x[1] = OUT15R(ai + bi, s);
    x[2] = OUT15R(ar - br, s);
    x[3] = OUT15R(ai - bi, s);
    x += 4;
  }
}

void fd_fft15_t2l(int16_t *x, uint32_t n, int32_t sh) {
  const uint32_t s = (uint32_t)-sh;
  for (uint32_t k = n >> 1; k != 0U; k--) {
    int32_t ar = x[0], ai = x[1], br = x[2], bi = x[3];
    x[0] = OUT15L(ar + br, s);
    x[1] = OUT15L(ai + bi, s);
    x[2] = OUT15L(ar - br, s);
    x[3] = OUT15L(ai - bi, s);
    x += 4;
  }
}

/* the j = 0 butterflies of stage q (W = 1): the rounding sits in x0 */
#define T4_BODY15(OUT, rnd)                                                    \
  const uint32_t qs = 2U * q;                                                  \
  for (uint32_t g = n >> (lg2(q) + 2U); g != 0U; g--) {                        \
    int32_t x0r = x[0] + (rnd), x0i = x[1] + (rnd);                            \
    int32_t x1r = x[qs], x1i = x[qs + 1U];                                     \
    int32_t x2r = x[2U * qs], x2i = x[2U * qs + 1U];                           \
    int32_t x3r = x[3U * qs], x3i = x[3U * qs + 1U];                           \
    int32_t ar = x0r + x1r, ai = x0i + x1i, br = x0r - x1r, bi = x0i - x1i;    \
    int32_t cr = x2r + x3r, ci = x2i + x3i, dr = x2r - x3r, di = x2i - x3i;    \
    x[0] = OUT(ar + cr, s);                                                    \
    x[1] = OUT(ai + ci, s);                                                    \
    x[qs] = OUT(br + di, s);                                                   \
    x[qs + 1U] = OUT(bi - dr, s);                                              \
    x[2U * qs] = OUT(ar - cr, s);                                              \
    x[2U * qs + 1U] = OUT(ai - ci, s);                                         \
    x[3U * qs] = OUT(br - di, s);                                              \
    x[3U * qs + 1U] = OUT(bi + dr, s);                                         \
    x += 4U * qs;                                                              \
  }

void fd_fft15_t4r(int16_t *x, uint32_t n, uint32_t q, int32_t sh) {
  const uint32_t s = (uint32_t)sh;
  const int32_t rnd = (1 << (s - 1U)) - 1;
  T4_BODY15(OUT15R, rnd)
}

void fd_fft15_t4l(int16_t *x, uint32_t n, uint32_t q, int32_t sh) {
  const uint32_t s = (uint32_t)-sh;
  T4_BODY15(OUT15L, 0)
}

/* cos, sin of a / (4 QP) turns, 0 < a < 2 QP (with j <= q/2, W^3j stays
 * under 1.5 QP), rounded to 2^15 */
FX_INLINE void tw15(uint32_t a, int32_t *c, int32_t *s) {
  if (a <= QP) {
    *s = fd_fft_tw15(TAB[a]);
    *c = fd_fft_tw15(TAB[QP - a]);
  } else {
    *s = fd_fft_tw15(TAB[2U * QP - a]);
    *c = -fd_fft_tw15(TAB[a - QP]);
  }
}

/* one j's butterflies down the groups. Sums at 2^15 product scale, exact in
 * int32 for b <= 13 (ps 0); for b = 14, 15 (ps 2) products enter as p >> 2 */
FX_INLINE void bfly15(int16_t *p, uint32_t ng, uint32_t qs, uint32_t k,
                      int32_t rnd, uint32_t ps, int32_t c1, int32_t s1,
                      int32_t c2, int32_t s2, int32_t c3, int32_t s3) {
  const int32_t one = 1 << (15U - ps);
  for (uint32_t g = ng; g != 0U; g--) {
    int32_t x1r = p[qs], x1i = p[qs + 1U];
    int32_t x2r = p[2U * qs], x2i = p[2U * qs + 1U];
    int32_t x3r = p[3U * qs], x3i = p[3U * qs + 1U];
    int32_t p1r = (x1r * c2 + x1i * s2) >> ps;
    int32_t p1i = (x1i * c2 - x1r * s2) >> ps;
    int32_t p2r = (x2r * c1 + x2i * s1) >> ps;
    int32_t p2i = (x2i * c1 - x2r * s1) >> ps;
    int32_t p3r = (x3r * c3 + x3i * s3) >> ps;
    int32_t p3i = (x3i * c3 - x3r * s3) >> ps;
    int32_t x0r = p[0] * one + rnd, x0i = p[1] * one + rnd;
    int32_t ar = x0r + p1r, ai = x0i + p1i, br = x0r - p1r, bi = x0i - p1i;
    int32_t cr = p2r + p3r, ci = p2i + p3i, dr = p2r - p3r, di = p2i - p3i;
    p[0] = OUT15R(ar + cr, k);
    p[1] = OUT15R(ai + ci, k);
    p[qs] = OUT15R(br + di, k);
    p[qs + 1U] = OUT15R(bi - dr, k);
    p[2U * qs] = OUT15R(ar - cr, k);
    p[2U * qs + 1U] = OUT15R(ai - ci, k);
    p[3U * qs] = OUT15R(br - di, k);
    p[3U * qs + 1U] = OUT15R(bi + dr, k);
    p += 4U * qs;
  }
}

/* j = 1..q-1, twiddle-major; j and q - j share one fold: W^(q-j) = (s, c),
 * W^2(q-j) = (-c2, s2), W^3(q-j) = (-s3, -c3) as (cos, sin) */
FX_INLINE void r4_15(int16_t *x, uint32_t n, uint32_t q, int32_t sh,
                     uint32_t ps) {
  const uint32_t k = (uint32_t)(15 + sh) - ps;
  const int32_t rnd = (1 << (k - 1U)) - 1;
  const uint32_t qs = 2U * q, ng = n >> (lg2(q) + 2U), step = QP >> lg2(q);
  for (uint32_t j = 1U; 2U * j <= q; j++) {
    int32_t c1, s1, c2, s2, c3, s3;
    uint32_t a = j * step;
    FX_OPAQUE(a); /* else six table pointers step with j, spilled */
    tw15(a, &c1, &s1);
    tw15(2U * a, &c2, &s2);
    tw15(3U * a, &c3, &s3);
    bfly15(x + 2U * j, ng, qs, k, rnd, ps, c1, s1, c2, s2, c3, s3);
    if (2U * j < q) {
      bfly15(x + 2U * (q - j), ng, qs, k, rnd, ps, s1, c1, -c2, s2, -s3, -c3);
    }
  }
}

NOINLINE void fd_fft15_r4(int16_t *x, uint32_t n, uint32_t q, int32_t sh) {
  r4_15(x, n, q, sh, 0U);
}

NOINLINE void fd_fft15_r4w(int16_t *x, uint32_t n, uint32_t q, int32_t sh) {
  r4_15(x, n, q, sh, 2U);
}

FX_INLINE bool zero15(const int16_t *x, uint32_t n) {
  uint32_t acc = 0U;
  for (uint32_t k = n; k != 0U; k--) {
    acc |= ld32(x);
    x += 2;
  }
  return acc == 0U;
}

/* stage shifts from the growth bounds: radix-2 x2, radix-4 x4, radix-4 with
 * twiddles (1 + 3 sqrt 2)(1 + 2^-15) < 2^2.4 */
int32_t fd_fft15_cfft(int16_t *x, uint32_t n) {
  uint32_t b = n > 2U ? bits15(rev15(x, n)) : fd_fft15_scan(x, n);
  if (b == 0U && zero15(x, n)) { /* the reversal left zeros as they were */
    return FD_FFT_ZERO;
  }
  int32_t e = 0;
  uint32_t q = 1U;
  if ((fx_clz(n) & 1U) == 0U) { /* log2 n odd */
    int32_t sh = (int32_t)b - 14;
    if (sh > 0) {
      fd_fft15_t2r(x, n, sh);
    } else {
      fd_fft15_t2l(x, n, sh);
    }
    e = sh;
    q = 2U;
    if (n > 2U) {
      b = fd_fft15_scan(x, n);
    }
  }
  for (; 4U * q <= n; q *= 4U) {
    int32_t sh = (int32_t)b - (q == 1U ? 13 : 12);
    if (sh > 0) {
      fd_fft15_t4r(x, n, q, sh);
    } else {
      fd_fft15_t4l(x, n, q, sh);
    }
    if (q > 1U) {
      if (b <= 13U) {
        fd_fft15_r4(x, n, q, sh);
      } else {
        fd_fft15_r4w(x, n, q, sh);
      }
    }
    e += sh;
    if (16U * q <= n) {
      b = fd_fft15_scan(x, n);
    }
  }
  return e;
}

/* ---- q31 ---- */

FX_INLINE uint32_t bits31(int32_t v) {
  return (uint32_t)v ^ ((uint32_t)v << 1);
}

uint32_t fd_fft31_scan(const int32_t *x, uint32_t n) {
  uint32_t acc = 0U;
  for (uint32_t k = n >> 2; k != 0U; k--) {
    acc |= bits31(x[0]) | bits31(x[1]) | bits31(x[2]) | bits31(x[3]) |
           bits31(x[4]) | bits31(x[5]) | bits31(x[6]) | bits31(x[7]);
    x += 8;
  }
  for (uint32_t k = n & 3U; k != 0U; k--) {
    acc |= bits31(x[0]) | bits31(x[1]);
    x += 2;
  }
  return fd_fft_bits(acc);
}

FX_INLINE void swap31(int32_t *x, uint32_t a, uint32_t b) {
  int32_t ur = x[2U * a], ui = x[2U * a + 1U];
  x[2U * a] = x[2U * b];
  x[2U * a + 1U] = x[2U * b + 1U];
  x[2U * b] = ur;
  x[2U * b + 1U] = ui;
}

FX_INLINE uint32_t bits2(const int32_t *v) {
  return bits31(v[0]) | bits31(v[1]);
}

/* as rev15 */
FX_INLINE uint32_t rev31(int32_t *x, uint32_t n) {
  const uint32_t h = n >> 1;
  const uint32_t s = fx_clz(n) + 1U;
  uint32_t acc = 0U;
  for (uint32_t i = 0U; i < h; i += 2U) {
    uint32_t r = fx_rbit(i) >> s;
    swap31(x, i + 1U, r + h);
    acc |= bits2(x + 2U * (i + 1U)) | bits2(x + 2U * (r + h));
    if (i <= r) {
      acc |= bits2(x + 2U * i) | bits2(x + 2U * (i + h + 1U));
      if (i < r) {
        acc |= bits2(x + 2U * r) | bits2(x + 2U * (r + h + 1U));
        swap31(x, i, r);
        swap31(x, i + h + 1U, r + h + 1U);
      }
    }
  }
  return acc;
}

/* n >= 4 */
uint32_t fd_fft31_rev(int32_t *x, uint32_t n) {
  return fd_fft_bits(rev31(x, n));
}

void fd_fft31_flip(int32_t *x, uint32_t n) {
  for (uint32_t a = 1U, b = n - 1U; a < b; a++, b--) {
    swap31(x, a, b);
  }
}

/* hi: the high word; else bits k..k+31, 1 <= k <= 31 */
FX_INLINE int32_t out31(int64_t v, uint32_t k, bool hi) {
  return hi ? (int32_t)(v >> 32)
            : (int32_t)(((uint32_t)v >> k) |
                        ((uint32_t)(v >> 32) << (32U - k)));
}

#define OUT31R(v, s) out31((v), (s), false)
#define OUT31L(v, s) (int32_t)((uint32_t)(v) << (s))

/* sh = 1 only: b = 31 */
void fd_fft31_t2r(int32_t *x, uint32_t n, int32_t sh) {
  const uint32_t s = (uint32_t)sh;
  const int64_t rnd = (1LL << (s - 1U)) - 1;
  for (uint32_t k = n >> 1; k != 0U; k--) {
    int64_t ar = x[0] + rnd, ai = x[1] + rnd, br = x[2], bi = x[3];
    x[0] = OUT31R(ar + br, s);
    x[1] = OUT31R(ai + bi, s);
    x[2] = OUT31R(ar - br, s);
    x[3] = OUT31R(ai - bi, s);
    x += 4;
  }
}

/* b <= 30: the sums fit int32 */
void fd_fft31_t2l(int32_t *x, uint32_t n, int32_t sh) {
  const uint32_t s = (uint32_t)-sh;
  for (uint32_t k = n >> 1; k != 0U; k--) {
    int32_t ar = x[0], ai = x[1], br = x[2], bi = x[3];
    x[0] = OUT31L(ar + br, s);
    x[1] = OUT31L(ai + bi, s);
    x[2] = OUT31L(ar - br, s);
    x[3] = OUT31L(ai - bi, s);
    x += 4;
  }
}

#define T4_BODY31(T, OUT, rnd)                                                 \
  const uint32_t qs = 2U * q;                                                  \
  for (uint32_t g = n >> (lg2(q) + 2U); g != 0U; g--) {                        \
    T x0r = x[0] + (rnd), x0i = x[1] + (rnd);                                  \
    T x1r = x[qs], x1i = x[qs + 1U];                                           \
    T x2r = x[2U * qs], x2i = x[2U * qs + 1U];                                 \
    T x3r = x[3U * qs], x3i = x[3U * qs + 1U];                                 \
    T ar = x0r + x1r, ai = x0i + x1i, br = x0r - x1r, bi = x0i - x1i;          \
    T cr = x2r + x3r, ci = x2i + x3i, dr = x2r - x3r, di = x2i - x3i;          \
    x[0] = OUT(ar + cr, s);                                                    \
    x[1] = OUT(ai + ci, s);                                                    \
    x[qs] = OUT(br + di, s);                                                   \
    x[qs + 1U] = OUT(bi - dr, s);                                              \
    x[2U * qs] = OUT(ar - cr, s);                                              \
    x[2U * qs + 1U] = OUT(ai - ci, s);                                         \
    x[3U * qs] = OUT(br - di, s);                                              \
    x[3U * qs + 1U] = OUT(bi + dr, s);                                         \
    x += 4U * qs;                                                              \
  }

/* the real outputs, then the imaginary ones: half the 64-bit sums live */
FX_INLINE void t4r31(int32_t *x, uint32_t n, uint32_t q, uint32_t s) {
  const uint32_t qs = 2U * q;
  const int64_t rnd = (1LL << (s - 1U)) - 1;
  for (uint32_t g = n >> (lg2(q) + 2U); g != 0U; g--) {
    int32_t *p0 = x, *p1 = x + qs, *p2 = x + 2U * qs, *p3 = x + 3U * qs;
    int32_t x2r = p2[0], x3r = p3[0], x2i = p2[1], x3i = p3[1];
    int64_t dr = (int64_t)x2r - x3r, di = (int64_t)x2i - x3i;
    int64_t cr = (int64_t)x2r + x3r;
    int64_t ar = (int64_t)p0[0] + p1[0] + rnd,
            br = (int64_t)p0[0] - p1[0] + rnd;
    p0[0] = OUT31R(ar + cr, s);
    p2[0] = OUT31R(ar - cr, s);
    p1[0] = OUT31R(br + di, s);
    p3[0] = OUT31R(br - di, s);
    int64_t ci = (int64_t)x2i + x3i;
    int64_t ai = (int64_t)p0[1] + p1[1] + rnd,
            bi = (int64_t)p0[1] - p1[1] + rnd;
    p0[1] = OUT31R(ai + ci, s);
    p2[1] = OUT31R(ai - ci, s);
    p1[1] = OUT31R(bi - dr, s);
    p3[1] = OUT31R(bi + dr, s);
    x += 4U * qs;
  }
}

/* sh is 1..3 (b 30, 31; stage bounds 4 and 5.25): constant shifts */
void fd_fft31_t4r(int32_t *x, uint32_t n, uint32_t q, int32_t sh) {
  if (sh == 1) {
    t4r31(x, n, q, 1U);
  } else if (sh == 2) {
    t4r31(x, n, q, 2U);
  } else {
    t4r31(x, n, q, 3U);
  }
}

/* sh <= 0 means b <= 29: the sums fit int32 */
void fd_fft31_t4l(int32_t *x, uint32_t n, uint32_t q, int32_t sh) {
  const uint32_t s = (uint32_t)-sh;
  T4_BODY31(int32_t, OUT31L, 0)
}

/* a table entry at 2^(31 - ts), rounded */
FX_INLINE uint32_t tws31(uint32_t t, uint32_t ts) {
  return ts == 0U ? t : (t + (1U << (ts - 1U))) >> ts;
}

/* -cos, -sin of a / (4 QP) turns, 0 < a < 2 QP: the one unit value a stage
 * meets, sin = 1, is INT32_MIN */
FX_INLINE void tw31(uint32_t a, uint32_t ts, int32_t *nc, int32_t *ns) {
  if (a <= QP) {
    *ns = (int32_t)(0U - tws31(TAB[a], ts));
    *nc = (int32_t)(0U - tws31(TAB[QP - a], ts));
  } else {
    *ns = (int32_t)(0U - tws31(TAB[2U * QP - a], ts));
    *nc = (int32_t)tws31(TAB[a - QP], ts);
  }
}

/* x 2^xs + rnd: hi (sh >= 1) xs = 32 - sh, else 31 */
FX_INLINE int64_t pre31(int32_t x, uint32_t sh, bool hi, int64_t rnd) {
  uint32_t lo = hi ? (uint32_t)x << (32U - sh) : (uint32_t)x << 31;
  int32_t h = hi ? x >> sh : x >> 1;
  return (int64_t)(((uint64_t)(uint32_t)h << 32) | lo) + rnd;
}

/* one j down the groups from (-cos, -sin): -Re P and Im P of P = W x; x2, x3
 * first, then each half of the outputs, so fewer 64-bit values are live */
FX_INLINE void bfly31(int32_t *p, uint32_t ng, uint32_t qs, uint32_t sh,
                      uint32_t k, int64_t rnd, bool hi, int32_t nc1,
                      int32_t ns1, int32_t nc2, int32_t ns2, int32_t nc3,
                      int32_t ns3) {
  for (uint32_t g = ng; g != 0U; g--) {
    int32_t x2r = p[2U * qs], x2i = p[2U * qs + 1U];
    int32_t x3r = p[3U * qs], x3i = p[3U * qs + 1U];
    int64_t m2r = fx_smlal(fx_smull(x2r, nc1), x2i, ns1);
    int64_t m3r = fx_smlal(fx_smull(x3r, nc3), x3i, ns3);
    int64_t p2i = fx_smlal(fx_smull(x2i, -nc1), x2r, ns1);
    int64_t p3i = fx_smlal(fx_smull(x3i, -nc3), x3r, ns3);
    int64_t ncr = m2r + m3r, mdr = m2r - m3r;
    int64_t ci = p2i + p3i, di = p2i - p3i;
    int32_t x1r = p[qs], x1i = p[qs + 1U];
    int64_t m1r = fx_smlal(fx_smull(x1r, nc2), x1i, ns2);
    int64_t x0r = pre31(p[0], sh, hi, rnd);
    int64_t ar = x0r - m1r, br = x0r + m1r;
    p[0] = out31(ar - ncr, k, hi);
    p[2U * qs] = out31(ar + ncr, k, hi);
    p[qs] = out31(br + di, k, hi);
    p[3U * qs] = out31(br - di, k, hi);
    int64_t p1i = fx_smlal(fx_smull(x1i, -nc2), x1r, ns2);
    int64_t x0i = pre31(p[1], sh, hi, rnd);
    int64_t ai = x0i + p1i, bi = x0i - p1i;
    p[1] = out31(ai + ci, k, hi);
    p[2U * qs + 1U] = out31(ai - ci, k, hi);
    p[qs + 1U] = out31(bi + mdr, k, hi);
    p[3U * qs + 1U] = out31(bi - mdr, k, hi);
    p += 4U * qs;
  }
}

/* hi (sh >= 1): twiddles at 2^(32 - sh), sums under 5.25 x 2^60, the output
 * the high word; else twiddles at 2^31, the output the sum >> (31 + sh) */
FX_INLINE void r4_31(int32_t *x, uint32_t n, uint32_t q, int32_t sh, bool hi) {
  const uint32_t ts = hi ? (uint32_t)sh - 1U : 0U;
  const uint32_t k = hi ? 32U : (uint32_t)(31 + sh);
  const uint32_t s = hi ? (uint32_t)sh : 0U;
  const int64_t rnd = (int64_t)((1ULL << (k - 1U)) - 1U);
  const uint32_t qs = 2U * q, ng = n >> (lg2(q) + 2U), step = QP >> lg2(q);
  for (uint32_t j = 1U; 2U * j <= q; j++) {
    int32_t nc1, ns1, nc2, ns2, nc3, ns3;
    uint32_t a = j * step;
    FX_OPAQUE(a); /* else six table pointers step with j, spilled */
    tw31(a, ts, &nc1, &ns1);
    tw31(2U * a, ts, &nc2, &ns2);
    tw31(3U * a, ts, &nc3, &ns3);
    bfly31(x + 2U * j, ng, qs, s, k, rnd, hi, nc1, ns1, nc2, ns2, nc3, ns3);
    if (2U * j < q) {
      bfly31(x + 2U * (q - j), ng, qs, s, k, rnd, hi, ns1, nc1, -nc2, ns2, -ns3,
             -nc3);
    }
  }
}

NOINLINE void fd_fft31_r4(int32_t *x, uint32_t n, uint32_t q, int32_t sh) {
  r4_31(x, n, q, sh, true);
}

NOINLINE void fd_fft31_r4x(int32_t *x, uint32_t n, uint32_t q, int32_t sh) {
  r4_31(x, n, q, sh, false);
}

FX_INLINE bool zero31(const int32_t *x, uint32_t n) {
  uint32_t acc = 0U;
  for (uint32_t k = 2U * n; k != 0U; k--) {
    acc |= (uint32_t)*x++;
  }
  return acc == 0U;
}

int32_t fd_fft31_cfft(int32_t *x, uint32_t n) {
  uint32_t b = n > 2U ? fd_fft_bits(rev31(x, n)) : fd_fft31_scan(x, n);
  if (b == 0U && zero31(x, n)) {
    return FD_FFT_ZERO;
  }
  int32_t e = 0;
  uint32_t q = 1U;
  if ((fx_clz(n) & 1U) == 0U) {
    int32_t sh = (int32_t)b - 30;
    if (sh > 0) {
      fd_fft31_t2r(x, n, sh);
    } else {
      fd_fft31_t2l(x, n, sh);
    }
    e = sh;
    q = 2U;
    if (n > 2U) {
      b = fd_fft31_scan(x, n);
    }
  }
  for (; 4U * q <= n; q *= 4U) {
    int32_t sh = (int32_t)b - (q == 1U ? 29 : 28);
    if (sh > 0) {
      fd_fft31_t4r(x, n, q, sh);
    } else {
      fd_fft31_t4l(x, n, q, sh);
    }
    if (q > 1U) {
      if (sh > 0) {
        fd_fft31_r4(x, n, q, sh);
      } else {
        fd_fft31_r4x(x, n, q, sh);
      }
    }
    e += sh;
    if (16U * q <= n) {
      b = fd_fft31_scan(x, n);
    }
  }
  return e;
}

/* ---- API ---- */

FX_INLINE int32_t log2u(uint32_t n) { return 31 - (int32_t)fx_clz(n); }

int32_t fd_cfft_q15(fd_cq15 *x, uint32_t n) {
  FX_ASSERT(fd_fft_size_ok(n, 2U));
  int32_t e = fd_fft15_cfft(FLAT(x), n);
  return e == FD_FFT_ZERO ? 0 : e;
}

int32_t fd_cfft_q31(fd_cq31 *x, uint32_t n) {
  FX_ASSERT(fd_fft_size_ok(n, 2U));
  int32_t e = fd_fft31_cfft(FLAT(x), n);
  return e == FD_FFT_ZERO ? 0 : e;
}

/* IDFT(x)[k] = DFT(x)[n - k] / n */
int32_t fd_icfft_q15(fd_cq15 *x, uint32_t n) {
  FX_ASSERT(fd_fft_size_ok(n, 2U));
  int32_t e = fd_fft15_cfft(FLAT(x), n);
  if (e == FD_FFT_ZERO) {
    return 0;
  }
  fd_fft15_flip(FLAT(x), n);
  return e - log2u(n);
}

int32_t fd_icfft_q31(fd_cq31 *x, uint32_t n) {
  FX_ASSERT(fd_fft_size_ok(n, 2U));
  int32_t e = fd_fft31_cfft(FLAT(x), n);
  if (e == FD_FFT_ZERO) {
    return 0;
  }
  fd_fft31_flip(FLAT(x), n);
  return e - log2u(n);
}

/* past these shifts every value rounds to 0 or saturates */
void fd_fft_rescale_q15(fd_cq15 *x, uint32_t n, int32_t e, int32_t to) {
  int64_t s = (int64_t)e - to;
  if (s < -15) {
    fd_fill_q15(0, FLAT(x), 2U * n);
  } else {
    fd_shift_q15(FLAT(x), s > 15 ? 15 : (int32_t)s, FLAT(x), 2U * n);
  }
}

void fd_fft_rescale_q31(fd_cq31 *x, uint32_t n, int32_t e, int32_t to) {
  int64_t s = (int64_t)e - to;
  if (s < -31) {
    fd_fill_q31(0, FLAT(x), 2U * n);
  } else {
    fd_shift_q31(FLAT(x), s > 31 ? 31 : (int32_t)s, FLAT(x), 2U * n);
  }
}
