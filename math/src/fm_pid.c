/**
 * @file fm_pid.c
 * @brief PID controllers in Q16.16 and float on exact sums of products
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_pid.h"
#include "fm_internal.h"
#include <string.h>

#define F32_QNAN 0x7FC00000U

/* ---- Q16.16 ---- */

/* the sum clamped to [lo, hi], |lo|, |hi| <= 2^47: |h| >= 3 2^27 means |sum| >
 * 2^62 (h has its sign), otherwise s is the exact sum (h is within 2 per
 * product, up to 4, plus 2^13 for the start value) */
FX_INLINE int64_t sum_clamp(SumQ31 a, int64_t lo, int64_t hi) {
  int64_t s = (int64_t)a.s;
  if ((uint32_t)a.h + 0x18000000U >= 0x30000000U) {
    s = a.h < 0 ? lo : hi;
  }
  return s < lo ? lo : (s > hi ? hi : s);
}

/* |v| <= 2^47 is far below what h has to resolve: h starts at 0 */
FX_INLINE SumQ31 sum_from(int64_t v) {
  SumQ31 a = {(uint64_t)v, 0};
  return a;
}

void fm_pid_init_q16(FM_PidQ16 *p, fm_q16 kp, fm_q16 ki, fm_q16 kd,
                     fm_q16 out_min, fm_q16 out_max) {
  FX_ASSERT(out_min <= out_max);
  p->kp = kp;
  p->ki = ki;
  p->kd = kd;
  p->out_min = out_min;
  p->out_max = out_max;
  p->i = 0;
  p->prev = 0;
}

void fm_pid_reset_q16(FM_PidQ16 *p, fm_q16 pv) {
  p->i = 0;
  p->prev = pv;
}

/* kp e as kp sp - kp pv: exact without a 33-bit e */
fm_q16 fm_pid_q16(FM_PidQ16 *p, fm_q16 sp, fm_q16 pv) {
  int64_t lo = (int64_t)p->out_min * 65536;
  int64_t hi = (int64_t)p->out_max * 65536;
  SumQ31 a = sum_from(p->i);
  sum_mac(&a, p->ki, sp);
  sum_msub(&a, p->ki, pv);
  int64_t i = sum_clamp(a, lo, hi);
  p->i = i;
  SumQ31 o = sum_from(i);
  sum_mac(&o, p->kp, sp);
  sum_msub(&o, p->kp, pv);
  sum_mac(&o, p->kd, p->prev);
  sum_msub(&o, p->kd, pv);
  p->prev = pv;
  /* lo and hi are multiples of 2^16: clamping before rounding is the same */
  return (fm_q16)((sum_clamp(o, lo, hi) + 32768) >> 16);
}

/* ---- float ---- */

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

/* order-preserving integer for float bits (+-0 equal) */
FX_INLINE int32_t fkey(uint32_t b) {
  return (int32_t)b < 0 ? -(int32_t)(b & 0x7FFFFFFFU) : (int32_t)b;
}

FX_INLINE uint32_t fclamp(uint32_t x, uint32_t lo, uint32_t hi) {
  int32_t k = fkey(x);
  return k < fkey(lo) ? lo : (k > fkey(hi) ? hi : x);
}

void fm_pid_init_f32(FM_PidF32 *p, float kp, float ki, float kd, float out_min,
                     float out_max) {
  FX_ASSERT(fkey(f2u(out_min)) <= fkey(f2u(out_max)));
  p->kp = kp;
  p->ki = ki;
  p->kd = kd;
  p->out_min = out_min;
  p->out_max = out_max;
  p->i = 0.0f;
  p->prev = 0.0f;
}

void fm_pid_reset_f32(FM_PidF32 *p, float pv) {
  p->i = 0.0f;
  p->prev = pv;
}

/* B side unpacked << 5 with its exponent one up (the << 6 scale, halved): five
 * products stay below 2^63 */
FX_INLINE int32_t unp_b5(uint32_t b, int32_t *e, uint32_t *bad) {
  int32_t m = unp_f32(b, 5U, e, bad);
  (*e)++;
  return m;
}

float fm_pid_f32(FM_PidF32 *p, float sp, float pv) {
  uint32_t bad = 0U;
  int32_t esp;
  int32_t epv;
  int32_t epr;
  int32_t ei;
  int32_t ekp;
  int32_t eki;
  int32_t ekd;
  int32_t msp = unp_b5(f2u(sp), &esp, &bad);
  int32_t mpv = unp_b5(f2u(pv), &epv, &bad);
  int32_t mpr = unp_b5(f2u(p->prev), &epr, &bad);
  int32_t mi = unp_f32(f2u(p->i), 7U, &ei, &bad);
  int32_t mkp = unp_f32(f2u(p->kp), 7U, &ekp, &bad);
  int32_t mki = unp_f32(f2u(p->ki), 7U, &eki, &bad);
  int32_t mkd = unp_f32(f2u(p->kd), 7U, &ekd, &bad);
  if (bad != 0U) {
    return u2f(F32_QNAN);
  }
  uint32_t lo = f2u(p->out_min);
  uint32_t hi = f2u(p->out_max);
  /* I + ki sp - ki pv */
  const int32_t am[3] = {mi, mki, mki};
  const int32_t ae[3] = {ei, eki, eki};
  const int32_t bm[3] = {1 << 28, msp, -mpv};
  const int32_t be[3] = {128, esp, epv};
  uint32_t i = fclamp(sum_f32(am, ae, bm, be, 1U, 3U), lo, hi);
  memcpy(&p->i, &i, sizeof i);
  p->prev = pv;
  /* I + kp sp - kp pv + kd prev - kd pv */
  mi = unp_f32(i, 7U, &ei, &bad);
  const int32_t om[5] = {mi, mkp, mkp, mkd, mkd};
  const int32_t oe[5] = {ei, ekp, ekp, ekd, ekd};
  const int32_t pm[5] = {1 << 28, msp, -mpv, mpr, -mpv};
  const int32_t pe[5] = {128, esp, epv, epr, epv};
  return u2f(fclamp(sum_f32(om, oe, pm, pe, 1U, 5U), lo, hi));
}
