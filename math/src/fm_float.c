/**
 * @file fm_float.c
 * @brief Float functions on integer internals, for cores without an FPU
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_float.h"
#include "fm_internal.h"
#include "fu_bits.h"
#include <string.h>

#define F32_QNAN 0x7FC00000U

FX_INLINE uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

FX_INLINE float u2f(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

/* sqrt of m 2^(e - 150) as float bits, m8 = m << 8 */
FX_INLINE uint32_t sqrt_bits(uint32_t m8, int32_t e) {
  uint32_t hi = m8 >> ((uint32_t)e & 1U);
  uint32_t h = sqrt_norm32(hi);
  uint32_t s = (h << 16) + ((hi - h * h) << 15) / h;
  if ((s & 0xFFU) == 0x80U && fx_umull(s, s) > (uint64_t)hi << 32) {
    s--;
  }
  return ((uint32_t)((e + 125) >> 1) << 23) + ((s + 128U) >> 8);
}

/* +-0, subnormals, negatives, inf and NaN */
FX_COLD static float sqrtf_special(uint32_t b) {
  if ((b << 1) == 0U || b == 0x7F800000U) {
    return u2f(b);
  }
  if ((b & 0x7FFFFFFFU) > 0x7F800000U) {
    return u2f(b | 0x00400000U);
  }
  if ((int32_t)b < 0) {
    return u2f(F32_QNAN);
  }
  uint32_t lz = fx_clz(b); /* subnormal: b 2^-149, lz >= 9 */
  return u2f(sqrt_bits(b << lz, 9 - (int32_t)lz));
}

float fm_sqrtf(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return sqrtf_special(b);
  }
  return u2f(sqrt_bits((b << 8) | 0x80000000U, (int32_t)(b >> 23)));
}

/* round(2^47 / m), m in [2^23, 2^24) */
FX_INLINE uint32_t recip_mant(uint32_t m) {
  uint32_t y = 0xFFFFFFFFU / ((m >> 8) + 1U);
  uint32_t e = 0U - y * m;
  uint32_t q = (y << 8) + (uint32_t)(fx_umull(e, y) >> 32);
  if (0U - q * m >= m) {
    q++;
  }
  return (q + 1U) >> 1;
}

/* +-0, subnormals in and out, inf and NaN */
FX_COLD static float recipf_special(uint32_t b) {
  uint32_t sign = b & 0x80000000U;
  uint32_t a = b & 0x7FFFFFFFU;
  if (a >= 0x7F800000U) {
    return u2f(a == 0x7F800000U ? sign : b | 0x00400000U);
  }
  if (a == 0U) {
    return u2f(sign | 0x7F800000U);
  }
  uint32_t m = (a & 0x7FFFFFU) | 0x800000U;
  int32_t e = (int32_t)(a >> 23);
  if (e >= 253) { /* subnormal result, round(2^(299 - e) / m) */
    uint32_t rem;
    uint32_t q = udiv_norm(1ULL << (292 - e), m << 8, &rem);
    return u2f(sign | ((q + 1U) >> 1));
  }
  uint32_t lz = fx_clz(a) - 8U; /* subnormal input */
  e = 1 - (int32_t)lz;
  uint32_t r = ((uint32_t)(252 - e) << 23) + recip_mant(a << lz);
  return u2f(sign | (r >= 0x7F800000U ? 0x7F800000U : r));
}

float fm_recipf(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b << 1;
  if (a - 0x01000000U >= 0xFC000000U) { /* |x| < 2^-126 or >= 2^126 */
    return recipf_special(b);
  }
  uint32_t e = a >> 24;
  return u2f((b & 0x80000000U) + ((252U - e) << 23) +
             recip_mant((b & 0x7FFFFFU) | 0x800000U));
}

/* (2r + 1)^2 hi - 2^80, exact */
FX_INLINE int64_t rsqrt_side(uint32_t hi, uint32_t r) {
  uint64_t a = fx_umull(2U * r + 1U, 2U * r + 1U);
  return (int64_t)(fx_umull((uint32_t)a, hi) +
                   ((uint64_t)((uint32_t)(a >> 32) * hi) << 32));
}

/* 1/sqrt of m 2^(e - 150) as float bits, m8 = m << 8 */
FX_INLINE uint32_t rsqrt_bits(uint32_t m8, int32_t e) {
  uint32_t hi = m8 >> ((uint32_t)e & 1U);
  uint32_t y = (uint32_t)fm_rsqrt_seed[(hi >> 25) - 32U] << 16;
  for (int i = 0; i < 2; i++) {
    uint32_t y2 = (uint32_t)(fx_umull(y, y) >> 32);  /* 1/(4t), Q30 */
    uint32_t p = (uint32_t)(fx_umull(hi, y2) >> 32); /* t y^2, Q28 */
    y = (uint32_t)(fx_umull(y, (3U << 28) - p) >> 29);
  }
  uint32_t R = (y + 64U) >> 7;
  uint32_t g = (y - 38U) & 127U;
  if (g < 31U) {
    if (g < 26U) {
      R += rsqrt_side(hi, R) < 0 ? 1U : 0U;
    } else {
      R -= rsqrt_side(hi, R - 1U) > 0 ? 1U : 0U;
    }
  }
  return ((uint32_t)((378 - e) >> 1) << 23) + R;
}

/* +-0, subnormals, negatives, inf and NaN */
FX_COLD static float rsqrtf_special(uint32_t b) {
  if ((b << 1) == 0U) {
    return u2f(b | 0x7F800000U);
  }
  if ((b & 0x7FFFFFFFU) > 0x7F800000U) {
    return u2f(b | 0x00400000U);
  }
  if ((int32_t)b < 0) {
    return u2f(F32_QNAN);
  }
  if (b == 0x7F800000U) {
    return u2f(0U);
  }
  uint32_t lz = fx_clz(b);
  return u2f(rsqrt_bits(b << lz, 9 - (int32_t)lz));
}

float fm_rsqrtf(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return rsqrtf_special(b);
  }
  return u2f(rsqrt_bits((b << 8) | 0x80000000U, (int32_t)(b >> 23)));
}

#define F32_PI4 0x3F490FDBU /* the float above pi/4 */

/* |x| / (2 pi) mod 1 in Q64 turns, |x| = m 2^(e - 150) >= pi/4 */
FX_INLINE uint64_t trig_turns(uint32_t m, uint32_t e) {
  uint32_t pos = e - 118U;
  const uint32_t *w = &fm_inv2pi_tab[pos >> 5];
  uint32_t s = pos & 31U;
  uint32_t rs = 31U - s;
  uint32_t w0 = (w[0] << s) | ((w[1] >> 1) >> rs);
  uint32_t w1 = (w[1] << s) | ((w[2] >> 1) >> rs);
  uint32_t w2 = (w[2] << s) | ((w[3] >> 1) >> rs);
  uint64_t p = fx_umull(m, w1) + (fx_umull(m, w2) >> 32);
  return ((uint64_t)(m * w0 + (uint32_t)(p >> 32)) << 32) | (uint32_t)p;
}

/* z = r^2 in Q32, r = R 2^-E, R in [2^31, 2^32), E >= 32 */
FX_INLINE uint32_t trig_z(uint32_t R, uint32_t E) {
  uint32_t sh = 2U * E - 64U;
  uint32_t r2 = (uint32_t)(fx_umull(R, R) >> 32);
  return sh < 32U ? r2 >> sh : 0U;
}

FX_INLINE uint32_t umul_hi(uint32_t a, uint32_t b) {
  return (uint32_t)(fx_umull(a, b) >> 32);
}

/* sin r as float bits, r = R 2^-E <= pi/4: r (1 - z T(z)), T in Q31 */
FX_INLINE uint32_t sin_bits(uint32_t R, uint32_t E, uint32_t z) {
  const uint32_t *c = fm_sinf_poly;
  uint32_t t = c[2] - umul_hi(z, c[3]);
  t = c[1] - umul_hi(z, t);
  t = c[0] - umul_hi(z, t);
  uint32_t y = umul_hi(R, 0x80000000U - umul_hi(z, t)); /* top bit 29 or 30 */
  uint32_t n = fx_clz(y) - 1U;
  return ((157U - E - n) << 23) + (((y << n) + 64U) >> 7);
}

/* cos r as float bits, r <= pi/4: 1 - z U(z) in Q31, in [0.7, 1] */
FX_INLINE uint32_t cos_bits(uint32_t z) {
  const uint32_t *c = fm_cosf_poly;
  uint32_t u = c[2] - umul_hi(z, c[3]);
  u = c[1] - umul_hi(z, u);
  u = c[0] - umul_hi(z, u);
  return (125U << 23) + ((0x80000000U - umul_hi(z, u) + 64U) >> 7);
}

/* sin (pi/4) u as float bits, u = G 2^-E in (0, 1], v = u^2 in Q32 */
FX_INLINE uint32_t sin_u_bits(uint32_t G, uint32_t E, uint32_t v) {
  const uint32_t *c = fm_sinf_upoly;
  uint32_t t = c[3] - umul_hi(v, c[4]);
  t = c[2] - umul_hi(v, t);
  t = c[1] - umul_hi(v, t);
  uint32_t y = umul_hi(G, c[0] - umul_hi(v, t)); /* top bit 30 or 31 */
  uint32_t n = fx_clz(y);
  return ((157U - E - n) << 23) + ((((y << n) >> 7) + 1U) >> 1);
}

/* cos (pi/4) u as float bits: 1 - w, w = v U''(v) in Q32 */
FX_INLINE uint32_t cos_u_bits(uint32_t v) {
  const uint32_t *c = fm_cosf_upoly;
  uint32_t u = c[2] - umul_hi(v, c[3]);
  u = c[1] - umul_hi(v, u);
  u = c[0] - umul_hi(v, u);
  return (127U << 23) - ((umul_hi(v, u) + 127U) >> 8);
}

/* f in Q64 turns plus q quarter turns, reduced to u = 8h = G 2^-E, v = u^2 */
typedef struct {
  uint32_t G, E, v, q, cos;
} TrigArg;

FX_INLINE TrigArg trig_arg(uint64_t f, uint32_t q) {
  TrigArg t;
  q += (uint32_t)(f >> 62);
  uint64_t h = f & 0x3FFFFFFFFFFFFFFFULL;
  uint32_t mir = (uint32_t)(h >> 61);
  if (mir != 0U) {
    h = (1ULL << 62) - h;
  }
  uint32_t hi = (uint32_t)(h >> 32);
  uint32_t lz = fx_clz(hi);
  t.G = (hi << lz) | (((uint32_t)h >> 1) >> (31U - lz));
  t.E = 29U + lz;
  uint32_t sh = 2U * lz - 6U;
  uint32_t g2 = umul_hi(t.G, t.G);
  t.v = sh < 32U ? g2 >> sh : 0U;
  t.q = q;
  t.cos = (q ^ mir) & 1U;
  return t;
}

/* inf and NaN */
FX_COLD static float trigf_special(uint32_t b) {
  return u2f((b << 1) == 0xFF000000U ? F32_QNAN : b | 0x00400000U);
}

float fm_sinf(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b & 0x7FFFFFFFU;
  uint32_t sign = b & 0x80000000U;
  if (a < F32_PI4) {
    if (a < 0x00800000U) {
      return x;
    }
    uint32_t E = 158U - (a >> 23);
    uint32_t R = (a << 8) | 0x80000000U;
    return u2f(sign | sin_bits(R, E, trig_z(R, E)));
  }
  if (a >= 0x7F800000U) {
    return trigf_special(b);
  }
  TrigArg t = trig_arg(trig_turns((a & 0x7FFFFFU) | 0x800000U, a >> 23), 0U);
  return u2f((sign ^ (t.q & 2U) << 30) |
             (t.cos ? cos_u_bits(t.v) : sin_u_bits(t.G, t.E, t.v)));
}

float fm_cosf(float x) {
  uint32_t a = f2u(x) & 0x7FFFFFFFU;
  if (a < F32_PI4) {
    if (a < 0x00800000U) {
      return 1.0f;
    }
    uint32_t E = 158U - (a >> 23);
    return u2f(cos_bits(trig_z((a << 8) | 0x80000000U, E)));
  }
  if (a >= 0x7F800000U) {
    return trigf_special(f2u(x));
  }
  TrigArg t = trig_arg(trig_turns((a & 0x7FFFFFU) | 0x800000U, a >> 23), 1U);
  return u2f((t.q & 2U) << 30 |
             (t.cos ? cos_u_bits(t.v) : sin_u_bits(t.G, t.E, t.v)));
}

void fm_sincosf(float x, float *s, float *c) {
  uint32_t b = f2u(x);
  uint32_t a = b & 0x7FFFFFFFU;
  uint32_t sign = b & 0x80000000U;
  if (a < F32_PI4) {
    if (a < 0x00800000U) {
      *s = x;
      *c = 1.0f;
      return;
    }
    uint32_t E = 158U - (a >> 23);
    uint32_t R = (a << 8) | 0x80000000U;
    uint32_t z = trig_z(R, E);
    *s = u2f(sign | sin_bits(R, E, z));
    *c = u2f(cos_bits(z));
    return;
  }
  if (a >= 0x7F800000U) {
    *s = *c = trigf_special(b);
    return;
  }
  TrigArg t = trig_arg(trig_turns((a & 0x7FFFFFU) | 0x800000U, a >> 23), 0U);
  uint32_t sb = sin_u_bits(t.G, t.E, t.v);
  uint32_t cb = cos_u_bits(t.v);
  /* cos x = sin of a quarter turn more */
  *s = u2f((sign ^ (t.q & 2U) << 30) | (t.cos ? cb : sb));
  *c = u2f(((t.q + 1U) & 2U) << 30 | (t.cos ? sb : cb));
}

/* about n 2^32 / d for n < d, d >= 2^31: 0 to 7 units below the floor */
FX_INLINE uint32_t div_q32(uint32_t n, uint32_t d) {
  uint32_t y = 0xFFFFFFFFU / ((d >> 15) + 1U); /* 2^47 / d from below */
  uint32_t q = (uint32_t)(fx_umull(n, y) >> 15);
  uint64_t r = ((uint64_t)n << 32) - fx_umull(q, d);
  return q + (uint32_t)(fx_umull((uint32_t)(r >> 18), y) >> 29);
}

#define TAN8_Q32 1779033704U /* tan(pi/8) */
#define PI4_Q31 1686629713U
#define PI2_Q31 3373259426U
#define PI_Q30 3373259426U
#define F32_PI 0x40490FDBU
#define F32_PI2 0x3FC90FDBU
#define F32_3PI4 0x4016CBE4U

/* atan s / s = 1 - z T(z) in Q31, z = s^2 in Q32 */
FX_INLINE uint32_t atan_p(uint32_t z) {
  const uint32_t *c = fm_atanf_poly;
  uint32_t t = c[3] - umul_hi(z, c[4]);
  t = c[2] - umul_hi(z, t);
  t = c[1] - umul_hi(z, t);
  t = c[0] - umul_hi(z, t);
  return 0x80000000U - umul_hi(z, t);
}

/* float bits of v / 2^31, v >= 2^28 */
FX_INLINE uint32_t q31_bits(uint32_t v) {
  uint32_t n = fx_clz(v);
  return ((126U - n) << 23) + ((((v << n) >> 7) + 1U) >> 1);
}

/* atan2(y, x) as float bits, y, |x| = n 2^(e - 158), n in [2^31, 2^32) */
FX_INLINE uint32_t atan2_core(uint32_t ny, int32_t ey, uint32_t nx, int32_t ex,
                              uint32_t xneg) {
  uint32_t swap = ey > ex || (ey == ex && ny > nx);
  uint32_t mn = swap ? nx : ny;
  uint32_t mx = swap ? ny : nx;
  uint32_t d = (uint32_t)(swap ? ey - ex : ex - ey);
  uint32_t phi;
  if (d <= 2U && ((uint64_t)mn << 32) >> d > fx_umull(mx, TAN8_Q32)) {
    uint64_t m2 = (uint64_t)mx << d;
    uint64_t den = m2 + mn; /* [2^32, 2^35) */
    uint32_t sh = 32U - fx_clz((uint32_t)(den >> 32));
    uint32_t s = div_q32((uint32_t)((m2 - mn) >> sh), (uint32_t)(den >> sh));
    phi = PI4_Q31 - umul_hi(s, atan_p(umul_hi(s, s)));
  } else {
    /* t = s 2^(-32 - dd), s in [2^31, 2^32), dd >= 1 */
    uint32_t dd = d;
    if (mn >= mx) {
      mn >>= 1;
      dd--;
    }
    uint32_t s = div_q32(mn, mx);
    uint32_t zs = 2U * dd;
    uint32_t y = umul_hi(s, atan_p(zs < 32U ? umul_hi(s, s) >> zs : 0U));
    if ((swap | xneg) == 0U) { /* y 2^(-31 - dd): top bit 29 or 30 */
      uint32_t n = fx_clz(y) - 1U;
      int32_t ef = 126 - (int32_t)(dd + n);
      y <<= n;
      if (ef >= 1) {
        return ((uint32_t)(ef - 1) << 23) + ((y + 64U) >> 7);
      }
      uint32_t r = (uint32_t)(7 - ef); /* subnormal */
      return r < 32U ? ((y >> r) + 1U) >> 1 : 0U;
    }
    phi = dd < 32U ? y >> dd : 0U;
  }
  if (swap != 0U) {
    phi = PI2_Q31 - phi;
  }
  if (xneg != 0U) { /* >= pi/2: Q30 is enough */
    return q31_bits(PI_Q30 - (phi >> 1)) + (1U << 23);
  }
  return q31_bits(phi);
}

/* a float's magnitude as n 2^(e - 158), n in [2^31, 2^32); a != 0, finite */
FX_INLINE uint32_t atan_unpack(uint32_t a, int32_t *e) {
  if (a < 0x00800000U) {
    uint32_t lz = fx_clz(a);
    *e = 9 - (int32_t)lz;
    return a << lz;
  }
  *e = (int32_t)(a >> 23);
  return (a << 8) | 0x80000000U;
}

/* zeros, subnormals, inf and NaN */
FX_COLD static float atan2f_special(uint32_t by, uint32_t bx) {
  uint32_t ay = by & 0x7FFFFFFFU;
  uint32_t ax = bx & 0x7FFFFFFFU;
  uint32_t xneg = bx >> 31;
  uint32_t r;
  if (ay > 0x7F800000U || ax > 0x7F800000U) {
    return u2f((ay > 0x7F800000U ? by : bx) | 0x00400000U);
  }
  if (ay == 0x7F800000U) {
    r = ax != 0x7F800000U ? F32_PI2 : (xneg != 0U ? F32_3PI4 : F32_PI4);
  } else if (ax == 0x7F800000U || ay == 0U) {
    r = xneg != 0U ? F32_PI : 0U;
  } else if (ax == 0U) {
    r = F32_PI2;
  } else {
    int32_t ey;
    int32_t ex;
    uint32_t ny = atan_unpack(ay, &ey);
    uint32_t nx = atan_unpack(ax, &ex);
    r = atan2_core(ny, ey, nx, ex, xneg);
  }
  return u2f((by & 0x80000000U) | r);
}

float fm_atan2f(float y, float x) {
  uint32_t by = f2u(y);
  uint32_t bx = f2u(x);
  uint32_t ay = by & 0x7FFFFFFFU;
  uint32_t ax = bx & 0x7FFFFFFFU;
  if (ay - 0x00800000U >= 0x7F000000U || ax - 0x00800000U >= 0x7F000000U) {
    return atan2f_special(by, bx);
  }
  return u2f((by & 0x80000000U) |
             atan2_core((ay << 8) | 0x80000000U, (int32_t)(ay >> 23),
                        (ax << 8) | 0x80000000U, (int32_t)(ax >> 23), bx >> 31));
}

/* sqrt(1 - x^2) as n 2^(e - 158), |x| in [2^-126, 1) */
FX_INLINE uint32_t asin_root(uint32_t m, uint32_t ex, int32_t *e) {
  uint64_t sq = fx_umull(m, m);
  int32_t sh = 2 * (int32_t)ex - 238;
  uint64_t x2 = sh >= 0 ? sq << sh : (sh > -64 ? sq >> -sh : 0U);
  uint64_t w = (1ULL << 62) - x2;
  uint32_t lz = fu_clz64(w) & 62U;
  *e = 127 - (int32_t)(lz >> 1);
  return sqrt_norm64(w << lz);
}

/* |x| >= 1, zeros, subnormals and NaN: the value for asin (acos: 1) */
FX_COLD static float asinf_special(uint32_t b, uint32_t acos) {
  uint32_t a = b & 0x7FFFFFFFU;
  uint32_t neg = b & 0x80000000U;
  if (a > 0x7F800000U) {
    return u2f(b | 0x00400000U);
  }
  if (a > 0x3F800000U) {
    return u2f(F32_QNAN);
  }
  if (a == 0x3F800000U) {
    return u2f(acos != 0U ? (neg != 0U ? F32_PI : 0U) : neg | F32_PI2);
  }
  return u2f(acos != 0U ? F32_PI2 : b); /* |x| < 2^-126 */
}

float fm_asinf(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b & 0x7FFFFFFFU;
  if (a - 0x00800000U >= 0x3F000000U) { /* not normal, or |x| >= 1 */
    return asinf_special(b, 0U);
  }
  int32_t ec;
  uint32_t m = (a & 0x7FFFFFU) | 0x800000U;
  uint32_t c = asin_root(m, a >> 23, &ec);
  return u2f((b & 0x80000000U) |
             atan2_core(m << 8, (int32_t)(a >> 23), c, ec, 0U));
}

float fm_acosf(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b & 0x7FFFFFFFU;
  if (a - 0x00800000U >= 0x3F000000U) { /* not normal, or |x| >= 1 */
    return asinf_special(b, 1U);
  }
  int32_t ec;
  uint32_t m = (a & 0x7FFFFFU) | 0x800000U;
  uint32_t c = asin_root(m, a >> 23, &ec);
  return u2f(atan2_core(c, ec, m << 8, (int32_t)(a >> 23), b >> 31));
}

/* |result| = n 2^(e - 158), n in [2^31, 2^32) */
typedef struct {
  uint32_t n, e, neg;
} LogVal;

/* log2(1 + u) / u - 1 in Q31, u in Q38, |u| <= 2^-7; the u^3 term by MUL */
FX_INLINE int32_t log2_p1(int32_t u) {
  const uint32_t *c = fm_log2f_poly;
  int32_t t = (int32_t)c[2] - (((u >> 16) * (int32_t)(c[3] >> 16)) >> 6);
  t = (int32_t)c[1] - (fx_smull_hi(u, t) >> 6);
  return (int32_t)c[0] - (fx_smull_hi(u, t) >> 6);
}

/* log2(1 + d 2^-24), 0 < |d| <= 2^17: relative, from the normalised d */
FX_INLINE LogVal log2_near1(int32_t d) {
  LogVal r;
  uint32_t ad = mag(d);
  uint32_t lz = fx_clz(ad);
  uint32_t p = 0x80000000U + (uint32_t)log2_p1(d * 16384);
  uint32_t h = umul_hi(ad << lz, p); /* top bit 30 or 31 */
  uint32_t j = fx_clz(h);
  r.n = h << j;
  r.e = 135U - lz - j;
  r.neg = (uint32_t)d >> 31;
  return r;
}

/* log2 x, x = nm 2^(E - 31), |x - 1| >= 2^-7: E + T_k + log2(1 + u), Q40 */
FX_INLINE LogVal log2_table(uint32_t nm, int32_t E) {
  uint32_t k = (nm >> 25) & 63U;
  int32_t u =
      (int32_t)((int64_t)(fx_umull(nm, fm_log2_rcp[k]) - (1ULL << 63)) >> 25);
  int64_t l = ((int64_t)u * 2147483648LL + fx_smull(u, log2_p1(u))) >> 29;
  int64_t s = (int64_t)E * 1099511627776LL + ((int64_t)fm_log2_tab[k] << 8) +
              fm_log2_lo[k] + l;
  LogVal r;
  r.neg = (uint32_t)((uint64_t)s >> 63);
  uint64_t a = r.neg != 0U ? 0U - (uint64_t)s : (uint64_t)s;
  uint32_t hi = (uint32_t)(a >> 32);
  uint32_t lz = fx_clz(hi);
  r.n = (hi << lz) | ((uint32_t)a >> (32U - lz));
  r.e = 150U - lz;
  return r;
}

/* times ln 2 */
FX_INLINE LogVal ln_scale(LogVal r) {
  uint64_t p = fx_umull(r.n, LN2_Q32);
  if ((p >> 63) != 0U) {
    r.n = (uint32_t)(p >> 32);
  } else {
    r.n = (uint32_t)(p >> 31);
    r.e--;
  }
  return r;
}

FX_INLINE uint32_t log_bits(LogVal r) {
  return (r.neg << 31) + ((r.e - 1U) << 23) + (((r.n >> 7) + 1U) >> 1);
}

/* zeros, subnormals, negatives, inf and NaN */
FX_COLD static float logf_special(uint32_t b, uint32_t ln) {
  if ((b << 1) == 0U) {
    return u2f(0xFF800000U);
  }
  if ((b & 0x7FFFFFFFU) > 0x7F800000U) {
    return u2f(b | 0x00400000U);
  }
  if ((int32_t)b < 0) {
    return u2f(F32_QNAN);
  }
  if (b == 0x7F800000U) {
    return u2f(b);
  }
  uint32_t lz = fx_clz(b); /* subnormal: b 2^-149 */
  LogVal r = log2_table(b << lz, -118 - (int32_t)lz);
  return u2f(log_bits(ln != 0U ? ln_scale(r) : r));
}

/* x within 2^-7 of 1: x = 1 + d 2^-24 */
FX_INLINE int32_t near1_d(uint32_t b) {
  int32_t t = (int32_t)(b - 0x3F800000U);
  return t > 0 ? 2 * t : t;
}

float fm_log2f(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return logf_special(b, 0U);
  }
  if (b - 0x3F7E0000U < 0x00030000U) {
    int32_t d = near1_d(b);
    return d != 0 ? u2f(log_bits(log2_near1(d))) : 0.0f;
  }
  return u2f(
      log_bits(log2_table((b << 8) | 0x80000000U, (int32_t)(b >> 23) - 127)));
}

float fm_logf(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return logf_special(b, 1U);
  }
  if (b - 0x3F7E0000U < 0x00030000U) {
    int32_t d = near1_d(b);
    return d != 0 ? u2f(log_bits(ln_scale(log2_near1(d)))) : 0.0f;
  }
  return u2f(log_bits(
      ln_scale(log2_table((b << 8) | 0x80000000U, (int32_t)(b >> 23) - 127))));
}

#define LOG2E_HI 3098164009U /* log2 e in Q63 */
#define LOG2E_LO 1545072828U

/* 2^(i + f 2^-32) as float bits, -150 <= i < 128 */
FX_INLINE uint32_t exp2_bits(int32_t i, uint32_t f) {
  uint64_t v = exp2_q38(f >> 26, (f & 0x3FFFFFFU) << 6);
  if (i >= -126) {
    return ((uint32_t)(i + 126) << 23) + (uint32_t)((v + (1U << 14)) >> 15);
  }
  uint32_t s = (uint32_t)(-119 - i); /* 8..31 */
  return (((uint32_t)(v >> 7) >> s) + 1U) >> 1;
}

/* 2^(-(ip + f 2^-32)) as 2^i 2^(f' 2^-32) */
FX_INLINE int32_t exp2_neg(uint32_t ip, uint32_t *f) {
  int32_t i = -(int32_t)ip - (*f != 0U ? 1 : 0);
  *f = 0U - *f;
  return i;
}

/* NaN, inf, |x| < 2^-25 and |x| >= 128 */
FX_COLD static float exp2f_special(uint32_t b) {
  uint32_t a = b & 0x7FFFFFFFU;
  if (a > 0x7F800000U) {
    return u2f(b | 0x00400000U);
  }
  if (a < 0x33000000U) {
    return 1.0f;
  }
  if ((int32_t)b >= 0) {
    return u2f(0x7F800000U);
  }
  if (b >= 0xC3160000U) { /* x <= -150 */
    return 0.0f;
  }
  uint32_t f = b << 16; /* -150 < x <= -128: x = M 2^-16 */
  int32_t i = exp2_neg(((b & 0x7FFFFFU) | 0x800000U) >> 16, &f);
  return u2f(exp2_bits(i, f));
}

float fm_exp2f(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b << 1;
  if (a - 0x66000000U >= 0x20000000U) { /* |x| < 2^-25 or >= 128 */
    return exp2f_special(b);
  }
  int32_t E = (int32_t)(a >> 24) - 127;
  uint32_t y = (b << 8) | 0x80000000U;
  uint32_t ip = 0U;
  uint32_t f;
  if (E >= 0) {
    ip = y >> (31 - E);
    f = y << (E + 1);
  } else {
    f = y >> (-1 - E);
  }
  int32_t i = (int32_t)b < 0 ? exp2_neg(ip, &f) : (int32_t)ip;
  return u2f(exp2_bits(i, f));
}

/* NaN, inf, |x| < 2^-25 and |x| >= 128 */
FX_COLD static float expf_special(uint32_t b) {
  uint32_t a = b & 0x7FFFFFFFU;
  if (a > 0x7F800000U) {
    return u2f(b | 0x00400000U);
  }
  if (a < 0x33000000U) {
    return 1.0f;
  }
  return u2f((int32_t)b >= 0 ? 0x7F800000U : 0U);
}

float fm_expf(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b << 1;
  if (a - 0x66000000U >= 0x20000000U) {
    return expf_special(b);
  }
  /* |x| log2 e in Q32: y (Q31) times log2 e (Q63) is that times 2^(30 - E) */
  uint32_t y = (b << 8) | 0x80000000U;
  uint64_t p = fx_umull(y, LOG2E_HI) + umul_hi(y, LOG2E_LO);
  p >>= 157U - (a >> 24);
  uint32_t f = (uint32_t)p;
  int32_t i = (int32_t)(p >> 32);
  if ((int32_t)b < 0) {
    i = exp2_neg((uint32_t)i, &f);
    if (i < -150) {
      return 0.0f;
    }
  } else if (i >= 128) {
    return u2f(0x7F800000U);
  }
  return u2f(exp2_bits(i, f));
}
