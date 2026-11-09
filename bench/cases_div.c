/**
 * @file cases_div.c
 * @brief Bench cases for fm_div.h, with the other 64/32 divide cores, the
 *        usual idioms and soft-float (CMSIS-DSP's are in cases_cmsis.c)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fm_div.h"

/* ---- candidates: floor(n / d) and the remainder, d in [2^31, 2^32),
 * n < d 2^32 ---- */

FX_INLINE uint32_t mag(int32_t x) {
  return x < 0 ? 0U - (uint32_t)x : (uint32_t)x;
}

FX_INLINE uint32_t core_libgcc(uint64_t n, uint32_t d, uint32_t *rem) {
  uint32_t q = (uint32_t)(n / d);
  *rem = (uint32_t)n - q * d;
  return q;
}

/* the library's core: floor(m 2^16 / d), m < d 2^16 */
FX_INLINE uint32_t core_divlu(uint64_t m, uint32_t d, uint32_t *rem) {
  uint32_t nh = (uint32_t)(m >> 16);
  uint32_t n1 = (uint32_t)m & 0xFFFFU;
  uint32_t d1 = d >> 16;
  uint32_t d0 = d & 0xFFFFU;
  uint32_t q1 = nh / d1;
  uint32_t r = nh - q1 * d1;
  while (q1 * d0 > ((r << 16) | n1)) {
    q1--;
    r += d1;
    if (r > 0xFFFFU) {
      break;
    }
  }
  uint32_t u = ((nh << 16) | n1) - q1 * d;
  uint32_t q0 = u / d1;
  r = u - q0 * d1;
  while (q0 * d0 > (r << 16)) {
    q0--;
    r += d1;
    if (r > 0xFFFFU) {
      break;
    }
  }
  *rem = (u << 16) - q0 * d;
  return (q1 << 16) | q0;
}

/* floor(2^24 / (i + 257)): below 2^63 / d for d >> 23 = i + 256 */
#define SEED1(i) (uint16_t)(0x1000000U / ((i) + 257U)),
#define SEED4(i) SEED1(i) SEED1((i) + 1U) SEED1((i) + 2U) SEED1((i) + 3U)
#define SEED16(i) SEED4(i) SEED4((i) + 4U) SEED4((i) + 8U) SEED4((i) + 12U)
#define SEED64(i)                                                              \
  SEED16(i) SEED16((i) + 16U) SEED16((i) + 32U) SEED16((i) + 48U)
static const uint16_t recip_seed[256] FX_ALIGN_LINE = {
    SEED64(0U) SEED64(64U) SEED64(128U) SEED64(192U)};

/* y < 2^63 / d from the seed, a 32-bit Newton step (e from p + 1, so y stays
 * below) and one with the 64-bit error; q = n y >> 63, then add back */
FX_INLINE uint32_t core_recip(uint64_t n, uint32_t d, uint32_t *rem) {
  uint32_t y = (uint32_t)recip_seed[(d >> 23) - 256U] << 16;
  uint32_t e = 0x7FFFFFFFU - fx_umull_hi(d, y);
  y += fx_umull_hi(y, e) << 1;
  uint64_t e64 = (1ULL << 63) - (uint64_t)d * y;
  y += fx_umull_hi(y, (uint32_t)(e64 >> 16)) >> 15;
  uint64_t t = (uint64_t)(uint32_t)(n >> 32) * y + fx_umull_hi((uint32_t)n, y);
  uint32_t q = (uint32_t)(t >> 31);
  uint64_t r = n - (uint64_t)q * d;
  while (r >= d) {
    q++;
    r -= d;
  }
  *rem = (uint32_t)r;
  return q;
}

FX_INLINE int32_t apply_sign(uint32_t q, int32_t sign) {
  if (sign < 0) {
    return (int32_t)(0U - q);
  }
  return q > (uint32_t)INT32_MAX ? INT32_MAX : (int32_t)q;
}

FX_INLINE int32_t saturate(uint32_t ua, int32_t sign) {
  if (ua == 0U) {
    return 0;
  }
  return sign < 0 ? INT32_MIN : INT32_MAX;
}

/* fm_div_q31 with another core taking n */
#define DIV_Q31(name, core)                                                    \
  BENCH_CALL static int32_t name(int32_t a, int32_t b) {                       \
    uint32_t ua = mag(a);                                                      \
    uint32_t ub = mag(b);                                                      \
    int32_t sign = a ^ b;                                                      \
    if (ua >= ub) {                                                            \
      return saturate(ua, sign);                                               \
    }                                                                          \
    if (ub == 0x80000000U) {                                                   \
      return apply_sign(ua, sign);                                             \
    }                                                                          \
    uint32_t s = fx_clz(ub);                                                   \
    uint32_t d = ub << s;                                                      \
    uint32_t r;                                                                \
    uint32_t q = core((uint64_t)(ua << (s - 1U)) << 32, d, &r);                \
    return apply_sign(q + (r >= d - r ? 1U : 0U), sign);                       \
  }

DIV_Q31(div31_libgcc, core_libgcc)
DIV_Q31(div31_recip, core_recip)

/* fm_div_q16 without the 32-bit path for |a| < 1 */
BENCH_CALL static int32_t div16_core_only(int32_t a, int32_t b) {
  uint32_t ua = mag(a);
  uint32_t ub = mag(b);
  int32_t sign = a ^ b;
  if ((ua >> 15) >= ub) {
    return saturate(ua, sign);
  }
  uint32_t s = fx_clz(ub);
  uint32_t d = ub << s;
  uint32_t r;
  uint32_t q = core_divlu((uint64_t)ua << s, d, &r);
  return apply_sign(q + (r >= d - r ? 1U : 0U), sign);
}

/* the usual idioms: truncating, no saturation, b != 0 */
BENCH_CALL static int16_t div15_trunc(int16_t a, int16_t b) {
  return (int16_t)FX_SSAT((a * 32768) / b, 16);
}

BENCH_CALL static int32_t div16_int64(int32_t a, int32_t b) {
  return (int32_t)((int64_t)a * 65536 / b);
}

/* shift a up by its headroom (at most 16), b down by the rest: inexact; for
 * |a| < 2^30 */
BENCH_CALL static int32_t div16_preshift(int32_t a, int32_t b) {
  uint32_t k = fx_clz(mag(a)) - 1U;
  k = k > 16U ? 16U : k;
  int32_t d = b >> (16U - k);
  if (d == 0) {
    return (a ^ b) < 0 ? INT32_MIN : INT32_MAX;
  }
  return (int32_t)((uint32_t)a << k) / d;
}

BENCH_CALL static int32_t recip16_int64(int32_t x) {
  return (int32_t)(0x100000000LL / x);
}

/* ---- inputs: mostly quotients in range ---- */

static inline int16_t in_q15_num(uint32_t u) {
  return (int16_t)((int16_t)(u >> 16) >> (1U + (u & 7U)));
}
static inline int16_t in_q15_den(uint32_t u) {
  return (int16_t)((u >> 16) | 1U);
}
static inline int32_t in_q31_num(uint32_t u) {
  return (int32_t)u >> (1U + (u & 15U));
}
static inline int32_t in_q31_den(uint32_t u) {
  return (int32_t)(u | 1U) >> (u & 3U);
}
/* 2^-16 .. 128 in magnitude, never 0 */
static inline int32_t in_q16(uint32_t u) {
  return ((int32_t)u >> (8U + (u & 15U))) | 1;
}

BENCH_BINARY(d_q15, int16_t, in_q15_num, int16_t, in_q15_den, int16_t,
             fm_div_q15(a, b))
BENCH_BINARY(d_q15_trunc, int16_t, in_q15_num, int16_t, in_q15_den, int16_t,
             div15_trunc(a, b))
BENCH_BINARY(d_q31, int32_t, in_q31_num, int32_t, in_q31_den, int32_t,
             fm_div_q31(a, b))
BENCH_BINARY(d_q31_libgcc, int32_t, in_q31_num, int32_t, in_q31_den, int32_t,
             div31_libgcc(a, b))
BENCH_BINARY(d_q31_recip, int32_t, in_q31_num, int32_t, in_q31_den, int32_t,
             div31_recip(a, b))
BENCH_BINARY(d_q16, int32_t, in_q16, int32_t, in_q16, int32_t, fm_div_q16(a, b))
BENCH_BINARY(d_q16_core, int32_t, in_q16, int32_t, in_q16, int32_t,
             div16_core_only(a, b))
BENCH_BINARY(d_q16_int64, int32_t, in_q16, int32_t, in_q16, int32_t,
             div16_int64(a, b))
BENCH_BINARY(d_q16_pre, int32_t, in_q16, int32_t, in_q16, int32_t,
             div16_preshift(a, b))
BENCH_BINARY(d_q16_sf, int32_t, in_q16, int32_t, in_q16, int32_t,
             fm_f32_to_q16((float)a / (float)b))
BENCH_UNARY(d_rq16, int32_t, in_q16, int32_t, fm_recip_q16(x))
BENCH_UNARY(d_rq16_div, int32_t, in_q16, int32_t, fm_div_q16(FM_Q16_ONE, x))
BENCH_UNARY(d_rq16_int64, int32_t, in_q16, int32_t, recip16_int64(x))
BENCH_UNARY(d_rq16_sf, int32_t, in_q16, int32_t,
            fm_f32_to_q16(65536.0f / (float)x))

/* the truncating and float rows differ from flexmath's results, but are the
 * same on both builds */
static const BenchCase cases[] = {
    {"fm_div_q15", "flexmath", d_q15, true},
    {"(a << 15) / b, truncating", "candidate", d_q15_trunc, true},
    {"fm_div_q31", "flexmath", d_q31, true},
    {"div q31, libgcc 64-bit core", "candidate", d_q31_libgcc, true},
    {"div q31, seed + Newton reciprocal core", "candidate", d_q31_recip, true},
    {"fm_div_q16", "flexmath", d_q16, true},
    {"div q16 without the 32-bit path", "candidate", d_q16_core, true},
    {"(int64_t)a * 65536 / b, truncating", "libgcc", d_q16_int64, true},
    {"div q16 pre-shift SDIV (inexact)", "candidate", d_q16_pre, true},
    {"a / b in float, to Q16.16", "soft-float", d_q16_sf, true},
    {"fm_recip_q16", "flexmath", d_rq16, true},
    {"fm_div_q16(1, x)", "candidate", d_rq16_div, true},
    {"2^32 / (int64_t)x, truncating", "libgcc", d_rq16_int64, true},
    {"1 / x in float, to Q16.16", "soft-float", d_rq16_sf, true},
};

const BenchGroup bench_group_div = {cases, BENCH_COUNT(cases)};
