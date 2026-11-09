/**
 * @file cases_float.c
 * @brief Bench cases for fm_float.h, with candidates and newlib's libm on the
 *        same inputs
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fm_float.h"
#include "fm_internal.h"
#include "fu_bits.h"
#include <math.h>

static inline uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

static inline float u2f(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

/* ---- candidates (specials go to fm_sqrtf; the inputs have none) ---- */

/* fm_sqrtf with the s^2 test on every input */
BENCH_CALL static float sqrtf_fix_all(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return fm_sqrtf(x);
  }
  uint32_t e = b >> 23;
  uint32_t hi = ((b << 8) | 0x80000000U) >> (e & 1U);
  uint32_t h = sqrt_norm32(hi);
  uint32_t s = (h << 16) + ((hi - h * h) << 15) / h;
  if (fx_umull(s, s) > (uint64_t)hi << 32) {
    s--;
  }
  return u2f((((e + 125U) >> 1) << 23) + ((s + 128U) >> 8));
}

/* divide-free: 1/sqrt seed, two UMULL Newton steps, sqrt = hi / sqrt(hi), then
 * the nearest by exact squares against 4N */
BENCH_CALL static float sqrtf_rsqrt(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return fm_sqrtf(x);
  }
  uint32_t e = b >> 23;
  uint32_t hi = ((b << 8) | 0x80000000U) >> (e & 1U);
  uint32_t y = (uint32_t)fm_rsqrt_seed[(hi >> 25) - 32U] << 16;
  for (int i = 0; i < 2; i++) {
    uint32_t y2 = (uint32_t)(fx_umull(y, y) >> 32);
    uint32_t p = (uint32_t)(fx_umull(hi, y2) >> 32);
    y = (uint32_t)(fx_umull(y, (3U << 28) - p) >> 29);
  }
  uint32_t r = ((uint32_t)(fx_umull(hi, y) >> 30) + 128U) >> 8;
  uint64_t n4 = (uint64_t)hi << 18;
  uint64_t up = 2U * (uint64_t)r + 1U;
  if (up * up < n4) {
    r++;
  } else {
    uint64_t dn = 2U * (uint64_t)r - 1U;
    if (dn * dn > n4) {
      r--;
    }
  }
  return u2f((((e + 125U) >> 1) << 23) + r);
}

/* recipf: floor(2^48 / m) by divlu (two UDIV digits) */
BENCH_CALL static float recipf_divlu(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b << 1;
  if (a - 0x01000000U >= 0xFC000000U) {
    return fm_recipf(x);
  }
  uint32_t rem;
  uint32_t q = udiv_norm(1ULL << 40, (b << 8) | 0x80000000U, &rem);
  return u2f((b & 0x80000000U) + ((252U - (a >> 24)) << 23) + ((q + 1U) >> 1));
}

/* recipf without a divide: fm_log2_rcp seed (2^-7), two Newton steps in Q31,
 * then the floor by the remainder */
BENCH_CALL static float recipf_newton(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b << 1;
  if (a - 0x01000000U >= 0xFC000000U) {
    return fm_recipf(x);
  }
  uint32_t m = (b & 0x7FFFFFU) | 0x800000U;
  uint32_t y = fm_log2_rcp[(m >> 17) & 63U] >> 1; /* 2^54 / m */
  for (int i = 0; i < 2; i++) {
    int32_t d = (int32_t)((int64_t)((1ULL << 54) - fx_umull(m, y)) >> 22);
    y += (uint32_t)(fx_smull((int32_t)y, d) >> 32);
  }
  uint32_t q = y >> 6;
  int32_t r = (int32_t)(0U - q * m);
  if (r < 0) {
    q--;
  } else if ((uint32_t)r >= m) {
    q++;
  }
  return u2f((b & 0x80000000U) + ((252U - (a >> 24)) << 23) + ((q + 1U) >> 1));
}

/* rsqrtf: a third-order first step (y within (-1, 2.5)), so only 3 of 128
 * guard values need the exact test, (2R - 1)^2 hi against 2^80 */
static inline int32_t rsqrt_resid(uint32_t hi, uint32_t y) {
  uint32_t y2 = (uint32_t)(fx_umull(y, y) >> 32);
  return (int32_t)((1U << 30) - (uint32_t)(fx_umull(hi, y2) >> 30));
}

BENCH_CALL static float rsqrtf_third(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return fm_rsqrtf(x);
  }
  uint32_t e = b >> 23;
  uint32_t hi = ((b << 8) | 0x80000000U) >> (e & 1U);
  uint32_t y = (uint32_t)fm_rsqrt_seed[(hi >> 25) - 32U] << 16;
  int32_t d = rsqrt_resid(hi, y);
  int32_t c = (d >> 1) + (int32_t)((3 * (fx_smull(d, d) >> 30)) >> 3);
  y += (uint32_t)(fx_smull((int32_t)(y >> 1), c) >> 29);
  d = rsqrt_resid(hi, y);
  y += (uint32_t)(fx_smull((int32_t)(y >> 1), d) >> 30);
  uint32_t r = (y + 64U) >> 7;
  if (((y - 64U) & 127U) < 3U) {
    uint64_t a = fx_umull(2U * r - 1U, 2U * r - 1U);
    uint64_t p = fx_umull((uint32_t)a, hi) +
                 ((uint64_t)((uint32_t)(a >> 32) * hi) << 32);
    r -= (int64_t)p > 0 ? 1U : 0U;
  }
  return u2f((((378U - e) >> 1) << 23) + r);
}

/* atan2f's division as cores: floor(n 2^32 / d) for n < d, d >= 2^31 */
BENCH_CALL static uint32_t div_divlu(uint32_t n, uint32_t d) {
  uint32_t rem;
  return udiv_norm((uint64_t)n << 16, d, &rem);
}

/* y ~ 2^47 / d from below (16 bits) by one UDIV, then one remainder
 * correction: 0 to 7 units below the floor */
BENCH_CALL static uint32_t div_recip(uint32_t n, uint32_t d) {
  uint32_t y = 0xFFFFFFFFU / ((d >> 15) + 1U);
  uint32_t q = (uint32_t)(fx_umull(n, y) >> 15);
  uint64_t r = ((uint64_t)n << 32) - fx_umull(q, d);
  return q + (uint32_t)(fx_umull((uint32_t)(r >> 18), y) >> 29);
}

static inline uint32_t sincos_xor(float x) {
  float s, c;
  fm_sincosf(x, &s, &c);
  return f2u(s) ^ f2u(c);
}

/* log2f without a table: m in [sqrt(1/2), sqrt(2)), s = (m - 1) / (m + 1) by
 * div_recip, normalised; log2 m = s C(z), z = s^2, C / 4 degree 4 in Q32 */
static const uint32_t log2_atanh_c[5] = {3098164009U, 1032721314U, 619638780U,
                                         442030314U, 365854269U};

BENCH_CALL static float log2f_atanh(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U || b == 0x3F800000U) {
    return fm_log2f(x);
  }
  int32_t e = (int32_t)(b >> 23) - 127;
  uint32_t m = (b & 0x7FFFFFU) | 0x800000U;
  uint32_t mq = m << 7; /* Q30 */
  if (m >= 0xB504F3U) {
    mq = m << 6;
    e++;
  }
  uint32_t neg = mq < (1U << 30) ? 1U : 0U;
  uint32_t num = neg != 0U ? (1U << 30) - mq : mq - (1U << 30);
  uint32_t den = mq + (1U << 30);
  uint32_t ld = fx_clz(den);
  uint32_t ln = fx_clz(num);
  den <<= ld;
  num <<= ln;
  uint32_t jn = ln - ld; /* s = q 2^(-32 - jn) */
  if (num >= den) {
    num >>= 1;
    jn--;
  }
  uint32_t q = div_recip(num, den);
  uint32_t zs = 2U * jn;
  uint32_t z = zs < 32U ? (uint32_t)(fx_umull(q, q) >> 32) >> zs : 0U;
  const uint32_t *c = log2_atanh_c;
  uint32_t t = c[4];
  for (int k = 3; k >= 0; k--) {
    t = c[k] + (uint32_t)(fx_umull(z, t) >> 32);
  }
  uint32_t h = (uint32_t)(fx_umull(q, t) >> 32); /* L = h 2^(-30 - jn) */
  uint32_t n;
  uint32_t ef;
  if (e == 0) {
    uint32_t lz = fx_clz(h);
    n = h << lz;
    ef = 128U - jn - lz;
  } else {
    int64_t r = (int64_t)e * 4294967296LL;
    r += neg != 0U ? -(int64_t)(h >> (jn - 2U)) : (int64_t)(h >> (jn - 2U));
    neg = r < 0 ? 1U : 0U;
    uint64_t a = neg != 0U ? 0U - (uint64_t)r : (uint64_t)r;
    uint32_t lz = fu_clz64(a);
    n = (uint32_t)((a << lz) >> 32);
    ef = 158U - lz;
  }
  return u2f((neg << 31) + ((ef - 1U) << 23) + (((n >> 7) + 1U) >> 1));
}

/* exp2f without a table: 2^f on [0, 1) by a degree-7 polynomial, Q32 inside
 * and Q31 for the last two steps */
static const uint32_t exp2_poly_c[7] = {2977044496U, 1031764466U, 238392704U,
                                        41291889U,   5764977U,    616427U,
                                        92336U};

BENCH_CALL static float exp2f_poly(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b << 1;
  if (a - 0x66000000U >= 0x20000000U) {
    return fm_exp2f(x);
  }
  int32_t e = (int32_t)(a >> 24) - 127;
  uint32_t y = (b << 8) | 0x80000000U;
  uint32_t ip = 0U;
  uint32_t f;
  if (e >= 0) {
    ip = y >> (31 - e);
    f = y << (e + 1);
  } else {
    f = y >> (-1 - e);
  }
  int32_t i = (int32_t)ip;
  if ((int32_t)b < 0) {
    i = -i - (f != 0U ? 1 : 0);
    f = 0U - f;
  }
  if (i < -126) {
    return fm_exp2f(x);
  }
  const uint32_t *c = exp2_poly_c;
  uint32_t t = c[6];
  for (int k = 5; k >= 1; k--) {
    t = c[k] + (uint32_t)(fx_umull(f, t) >> 32);
  }
  t = (c[0] >> 1) + (uint32_t)(fx_umull(f, t) >> 33);
  uint32_t p = 0x80000000U + (uint32_t)(fx_umull(f, t) >> 32);
  return u2f(((uint32_t)(i + 126) << 23) + (((p >> 7) + 1U) >> 1));
}

/* ---- inputs ---- */

/* positive normals 2^-31 .. 2^33, uniform in the exponent */
static inline float in_posf(uint32_t u) { return u2f(0x30000000U + (u >> 3)); }
static inline float in_subf(uint32_t u) { return u2f((u >> 9) | 1U); }
static inline float in_sgnf(uint32_t u) { return u2f((u << 31) | f2u(in_posf(u))); }
/* +-1, as the libm rows */
static inline float in_unit(uint32_t u) {
  return (float)(int32_t)u * 4.65661287e-10f;
}
static inline uint32_t in_num(uint32_t u) { return (u >> 1) | 0x40000000U; }
static inline uint32_t in_den(uint32_t u) { return u | 0x80000000U; }
/* +-pi, as the libm rows */
static inline float in_angle(uint32_t u) {
  return (float)(int32_t)u * 1.46291807926716e-9f;
}
/* within 2^-7 of 1 */
static inline float in_near1(uint32_t u) {
  return u2f(0x3F7E0000U + (u >> 8) % 0x30000U);
}
/* +-(2^-8 .. 64), uniform in the exponent */
static inline float in_expx(uint32_t u) {
  return u2f((u << 31) | (0x3B800000U + (u >> 1) % 0x07000000U));
}
/* +-(pi/4 .. 2^22), uniform in the exponent */
static inline float in_wide(uint32_t u) {
  return u2f((u << 31) | (0x3F490FDBU + (u >> 1) % 0x0AB6F025U));
}

BENCH_UNARY_COLD(f_sqrt, float, in_posf, float, fm_sqrtf(x))
BENCH_UNARY(f_sqrt_sub, float, in_subf, float, fm_sqrtf(x))
BENCH_UNARY(f_sqrt_fix_all, float, in_posf, float, sqrtf_fix_all(x))
BENCH_UNARY(f_sqrt_rsqrt, float, in_posf, float, sqrtf_rsqrt(x))
BENCH_UNARY_COLD(f_sqrt_libm, float, in_posf, float, sqrtf(x))
BENCH_UNARY_COLD(f_recip, float, in_sgnf, float, fm_recipf(x))
BENCH_UNARY(f_recip_sub, float, in_subf, float, fm_recipf(x))
BENCH_UNARY(f_recip_divlu, float, in_sgnf, float, recipf_divlu(x))
BENCH_UNARY(f_recip_newton, float, in_sgnf, float, recipf_newton(x))
BENCH_UNARY_COLD(f_recip_libgcc, float, in_sgnf, float, 1.0f / x)
BENCH_UNARY_COLD(f_rsqrt, float, in_posf, float, fm_rsqrtf(x))
BENCH_UNARY(f_rsqrt_sub, float, in_subf, float, fm_rsqrtf(x))
BENCH_UNARY(f_rsqrt_third, float, in_posf, float, rsqrtf_third(x))
BENCH_UNARY(f_rsqrt_composed, float, in_posf, float, fm_recipf(fm_sqrtf(x)))
BENCH_UNARY_COLD(f_rsqrt_libm, float, in_posf, float, 1.0f / sqrtf(x))
BENCH_UNARY_COLD(f_sin, float, in_angle, float, fm_sinf(x))
BENCH_UNARY(f_sin_wide, float, in_wide, float, fm_sinf(x))
BENCH_UNARY(f_sin_libm_wide, float, in_wide, float, sinf(x))
BENCH_UNARY(f_cos, float, in_angle, float, fm_cosf(x))
BENCH_UNARY_COLD(f_sincos, float, in_angle, uint32_t, sincos_xor(x))
BENCH_UNARY(f_sincos_split, float, in_angle, uint32_t,
            f2u(fm_sinf(x)) ^ f2u(fm_cosf(x)))
BENCH_BINARY_COLD(f_atan2, float, in_unit, float, in_unit, float,
                  fm_atan2f(a, b))
BENCH_BINARY(f_atan2_wide, float, in_sgnf, float, in_sgnf, float,
             fm_atan2f(a, b))
BENCH_BINARY(f_div_divlu, uint32_t, in_num, uint32_t, in_den, uint32_t,
             div_divlu(a, b))
BENCH_BINARY(f_div_recip, uint32_t, in_num, uint32_t, in_den, uint32_t,
             div_recip(a, b))
BENCH_UNARY_COLD(f_asin, float, in_unit, float, fm_asinf(x))
BENCH_UNARY(f_acos, float, in_unit, float, fm_acosf(x))
BENCH_UNARY_COLD(f_log2, float, in_posf, float, fm_log2f(x))
BENCH_UNARY(f_log2_near1, float, in_near1, float, fm_log2f(x))
BENCH_UNARY(f_log2_atanh, float, in_posf, float, log2f_atanh(x))
BENCH_UNARY(f_log2_libm, float, in_posf, float, log2f(x))
BENCH_UNARY(f_ln, float, in_posf, float, fm_logf(x))
BENCH_UNARY(f_ln_libm, float, in_posf, float, logf(x))
BENCH_UNARY_COLD(f_exp2, float, in_expx, float, fm_exp2f(x))
BENCH_UNARY(f_exp2_poly, float, in_expx, float, exp2f_poly(x))
BENCH_UNARY(f_exp2_libm, float, in_expx, float, exp2f(x))
BENCH_UNARY_COLD(f_exp, float, in_expx, float, fm_expf(x))
BENCH_UNARY_COLD(f_exp_libm, float, in_expx, float, expf(x))

static const BenchCase cases[] = {
    {"fm_sqrtf", "flexmath", f_sqrt, true},
    {"fm_sqrtf, subnormal inputs", "flexmath", f_sqrt_sub, true},
    {"sqrtf, s^2 test on every input", "candidate", f_sqrt_fix_all, true},
    {"sqrtf by divide-free 1/sqrt Newton", "candidate", f_sqrt_rsqrt, true},
    {"sqrtf, same inputs", "libm", f_sqrt_libm, false},
    {"fm_recipf", "flexmath", f_recip, true},
    {"fm_recipf, subnormal inputs", "flexmath", f_recip_sub, true},
    {"recipf by divlu (two UDIV digits)", "candidate", f_recip_divlu, true},
    {"recipf by Newton from a table seed", "candidate", f_recip_newton, true},
    {"1.0f / x", "libgcc", f_recip_libgcc, false},
    {"fm_rsqrtf", "flexmath", f_rsqrt, true},
    {"fm_rsqrtf, subnormal inputs", "flexmath", f_rsqrt_sub, true},
    {"rsqrtf, third-order step, 3/128 window", "candidate", f_rsqrt_third,
     true},
    {"fm_recipf(fm_sqrtf(x)) (two roundings)", "candidate", f_rsqrt_composed,
     true},
    {"1.0f / sqrtf(x)", "libm", f_rsqrt_libm, false},
    {"fm_sinf", "flexmath", f_sin, true},
    {"fm_sinf, |x| pi/4 .. 2^22", "flexmath", f_sin_wide, true},
    {"sinf, |x| pi/4 .. 2^22", "libm", f_sin_libm_wide, false},
    {"fm_cosf", "flexmath", f_cos, true},
    {"fm_sincosf", "flexmath", f_sincos, true},
    {"fm_sinf + fm_cosf", "flexmath", f_sincos_split, true},
    {"fm_atan2f", "flexmath", f_atan2, true},
    {"fm_atan2f, any magnitudes", "flexmath", f_atan2_wide, true},
    {"atan2f division core: divlu", "candidate", f_div_divlu, true},
    {"atan2f division core: UDIV reciprocal + correction (shipped)", "candidate",
     f_div_recip, true},
    {"fm_asinf", "flexmath", f_asin, true},
    {"fm_acosf", "flexmath", f_acos, true},
    {"fm_log2f", "flexmath", f_log2, true},
    {"fm_log2f, within 2^-7 of 1", "flexmath", f_log2_near1, true},
    {"log2f without a table (atanh series, one division)", "candidate",
     f_log2_atanh, true},
    {"log2f, same inputs", "libm", f_log2_libm, false},
    {"fm_logf", "flexmath", f_ln, true},
    {"logf, same inputs", "libm", f_ln_libm, false},
    {"fm_exp2f", "flexmath", f_exp2, true},
    {"exp2f without a table (degree-7 polynomial)", "candidate", f_exp2_poly,
     true},
    {"exp2f, same inputs", "libm", f_exp2_libm, false},
    {"fm_expf", "flexmath", f_exp, true},
    {"expf, same inputs", "libm", f_exp_libm, false},
};

const BenchGroup bench_group_float = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fm_sqrtf", "flexmath", f_sqrt_cold, true},
    {"sqrtf, same inputs", "libm", f_sqrt_libm_cold, false},
    {"fm_recipf", "flexmath", f_recip_cold, true},
    {"1.0f / x", "libgcc", f_recip_libgcc_cold, false},
    {"fm_rsqrtf", "flexmath", f_rsqrt_cold, true},
    {"1.0f / sqrtf(x)", "libm", f_rsqrt_libm_cold, false},
    {"fm_sinf", "flexmath", f_sin_cold, true},
    {"fm_sincosf", "flexmath", f_sincos_cold, true},
    {"fm_atan2f", "flexmath", f_atan2_cold, true},
    {"fm_asinf", "flexmath", f_asin_cold, true},
    {"fm_log2f", "flexmath", f_log2_cold, true},
    {"fm_exp2f", "flexmath", f_exp2_cold, true},
    {"fm_expf", "flexmath", f_exp_cold, true},
    {"expf, same inputs", "libm", f_exp_libm_cold, false},
};

const BenchGroup bench_group_float_cold = {cold, BENCH_COUNT(cold)};
