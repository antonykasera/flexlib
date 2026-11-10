/**
 * @file fm_exp.c
 * @brief log2 and exp2 cores, and the logs, exponentials and powers on them
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_exp.h"
#include "fm_internal.h"

/* log2(1 + u) = u (A1 - u (A2 - u (A3 - u A4))), A_i = 1 / (i ln 2) */
#define A1 1549082005 /* Q30 */
#define A2 1549082005 /* Q31 */
#define A3 1032721336 /* Q31 */
#define A4 774541002  /* Q31 */
#define LOG10_2_Q32 1292913986U
#define DB_Q29 3232284966U  /* 20 log10 2 */
#define LOG2E_HI 1549082004 /* log2 e in Q62, as hi:lo words */
#define LOG2E_LO 2920020062U

/* log2 of a Q16.16 x > 0, in Q32 */
FX_INLINE int64_t log2_core(uint32_t x) {
  uint32_t lz = fx_clz(x);
  uint32_t m = x << lz;
  uint32_t k = (m >> 25) & 63U;
  /* u to Q38 from m r = (1 + u) 2^63 */
  int32_t u =
      (int32_t)((int64_t)(fx_umull(m, fm_log2_rcp[k]) - (1ULL << 63)) >> 25);
  int32_t u16 = u >> 16;
  int32_t t = A3 - ((u16 * (A4 >> 15)) >> 7);
  t = A2 - ((u16 * (t >> 15)) >> 7);
  t = A1 - (fx_smull_hi(u, t) >> 7);
  int32_t s = fx_smull_hi(u, t); /* log2(1 + u), Q36 */
  return (int64_t)(15 - (int32_t)lz) * 4294967296LL + fm_log2_tab[k] +
         ((s + 8) >> 4);
}

/* round(v / 2^s) to an int32, s >= 1 */
FX_INLINE int32_t round_shift(int64_t v, uint32_t s) {
  return (int32_t)((v + (1LL << (s - 1U))) >> s);
}

/* L (Q32) times c (Q32 - shift), as Q16 */
FX_INLINE int32_t scale_log(int64_t L, uint32_t c, uint32_t shift) {
  int64_t hi = (L >> 32) * (int64_t)c;
  return round_shift(hi + fx_umull_hi((uint32_t)L, c), 16U - shift);
}

fm_q16 fm_log2_q16(fm_q16 x) {
  return x <= 0 ? INT32_MIN : round_shift(log2_core((uint32_t)x), 16U);
}

fm_q16 fm_ln_q16(fm_q16 x) {
  return x <= 0 ? INT32_MIN : scale_log(log2_core((uint32_t)x), LN2_Q32, 0U);
}

fm_q16 fm_log10_q16(fm_q16 x) {
  return x <= 0 ? INT32_MIN
                : scale_log(log2_core((uint32_t)x), LOG10_2_Q32, 0U);
}

fm_q16 fm_db_q16(fm_q16 x) {
  return x <= 0 ? INT32_MIN : scale_log(log2_core((uint32_t)x), DB_Q29, 3U);
}

/* 2^e for e = i + k/64 + r38 / 2^38, as Q16.16, rounded once */
FX_INLINE int32_t exp2_core(int32_t i, uint32_t k, uint32_t r38) {
  if (i >= 15) {
    return INT32_MAX;
  }
  if (i <= -17) { /* result 1/2 .. 1 LSB: rounds up */
    return i == -17 ? 1 : 0;
  }
  uint64_t v = exp2_q38(k, r38);
  uint32_t w = (uint32_t)(v >> 7) >> (14 - i);
  return (int32_t)((w + 1U) >> 1);
}

fm_q16 fm_exp2_q16(fm_q16 x) {
  uint32_t f = (uint32_t)x & 0xFFFFU;
  return exp2_core(x >> 16, f >> 10, (f & 1023U) << 22);
}

/* the exponent in Q46: x (Q16) times log2 e (Q62), exact to 2^-46 */
fm_q16 fm_exp_q16(fm_q16 x) {
  int64_t e = (int64_t)x * LOG2E_HI + (((int64_t)x * (int64_t)LOG2E_LO) >> 32);
  return exp2_core((int32_t)(e >> 46), (uint32_t)(e >> 40) & 63U,
                   (uint32_t)(e >> 8));
}

fm_q16 fm_pow_q16(fm_q16 x, fm_q16 y) {
  if (y == 0) {
    return FM_Q16_ONE;
  }
  if (x <= 0) {
    return x == 0 && y < 0 ? INT32_MAX : 0;
  }
  /* y log2 x in Q42 */
  int64_t L = log2_core((uint32_t)x);
  int64_t e = (int64_t)y * (int32_t)(L >> 6) + (((int64_t)y * (L & 63)) >> 6);
  return exp2_core((int32_t)(e >> 42), (uint32_t)(e >> 36) & 63U,
                   (uint32_t)(e >> 4));
}
