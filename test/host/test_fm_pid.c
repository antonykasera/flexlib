/**
 * @file test_fm_pid.c
 * @brief Host tests for fm_pid.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_pid.h"
#include "test_stats.h"
#include <string.h>

__extension__ typedef __int128 i128;
typedef long double ld;

static i128 clamp128(i128 v, i128 lo, i128 hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/* one step of the exact reference */
static fm_q16 ref_q16(FM_PidQ16 *r, fm_q16 sp, fm_q16 pv) {
  i128 lo = (i128)r->out_min * 65536;
  i128 hi = (i128)r->out_max * 65536;
  i128 i = clamp128((i128)r->i + (i128)r->ki * sp - (i128)r->ki * pv, lo, hi);
  r->i = (int64_t)i;
  i128 o = i + (i128)r->kp * sp - (i128)r->kp * pv + (i128)r->kd * r->prev -
           (i128)r->kd * pv;
  r->prev = pv;
  return (fm_q16)((clamp128(o, lo, hi) + 32768) >> 16);
}

/* full scale and small values both */
static fm_q16 rand_q16(uint64_t *st) {
  uint32_t u = test_rng_u32(st);
  switch (u & 7U) {
  case 0:
    return INT32_MIN;
  case 1:
    return INT32_MAX;
  case 2:
    return (fm_q16)(u >> 3) >> 12; /* +-16 or so */
  default:
    return (fm_q16)test_rng_u32(st);
  }
}

void test_fm_pid_q16(void) {
  /* behaviour: no derivative kick on a setpoint step, windup held at max */
  FM_PidQ16 p;
  fm_pid_init_q16(&p, FM_Q16_ONE, FM_Q16_ONE / 10, 50 * FM_Q16_ONE,
                  -10 * FM_Q16_ONE, 10 * FM_Q16_ONE);
  fm_pid_reset_q16(&p, 0);
  fm_q16 o = fm_pid_q16(&p, FM_Q16_ONE, 0); /* step: P + I only */
  ASSERT_EQ(o, FM_Q16_ONE + FM_Q16_ONE / 10);
  for (int k = 0; k < 1000; k++) {
    o = fm_pid_q16(&p, 100 * FM_Q16_ONE, 0);
  }
  ASSERT_EQ(o, 10 * FM_Q16_ONE);
  ASSERT_EQ(p.i, (int64_t)10 * FM_Q16_ONE * 65536); /* not beyond the limit */
  o = fm_pid_q16(&p, 0, 0); /* error gone: I alone, at once */
  ASSERT_EQ(o, 10 * FM_Q16_ONE);
  o = fm_pid_q16(&p, -FM_Q16_ONE, FM_Q16_ONE); /* pv jump: -50 kick from D */
  ASSERT_EQ(o, -10 * FM_Q16_ONE);

  /* random controllers, 32 steps each, against exact arithmetic */
  uint64_t st = 811U;
  for (int c = 0; c < 40000; c++) {
    fm_q16 a = rand_q16(&st);
    fm_q16 b = rand_q16(&st);
    fm_pid_init_q16(&p, rand_q16(&st), rand_q16(&st), rand_q16(&st),
                    a < b ? a : b, a < b ? b : a);
    fm_pid_reset_q16(&p, rand_q16(&st));
    FM_PidQ16 r = p;
    for (int k = 0; k < 32; k++) {
      fm_q16 sp = rand_q16(&st);
      fm_q16 pv = rand_q16(&st);
      fm_q16 got = fm_pid_q16(&p, sp, pv);
      ASSERT_EQ(got, ref_q16(&r, sp, pv));
      ASSERT_EQ(p.i, r.i);
    }
  }
}

static uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

static float u2f(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

/* |got - clamp(ref)| beyond half an ULP, in units of 2^-32 sum |terms| */
static double excess(float got, ld ref, ld sabs, float lo, float hi) {
  if (ref <= (ld)lo || ref >= (ld)hi) { /* rounding stays beyond: exactly */
    return f2u(got) == f2u(ref <= (ld)lo ? lo : hi) ? 0.0 : HUGE_VAL;
  }
  int e = 0;
  (void)frexpl(ref, &e);
  ld ulp = ldexpl(1.0L, e - 24 < -149 ? -149 : e - 24);
  ld d = fabsl((ld)got - ref) - ulp / 2.0L;
  return d <= 0.0L ? 0.0 : (double)(d / (sabs * 0x1p-32L));
}

/* +-[2^-8, 2^8), zero now and then */
static float rand_f32(uint64_t *st) {
  uint32_t u = test_rng_u32(st);
  if ((u & 15U) == 0U) {
    return 0.0f;
  }
  return u2f((u & 0x807FFFFFU) | ((119U + (u >> 23) % 16U) << 23));
}

void test_fm_pid_f32(void) {
  FM_PidF32 p;
  fm_pid_init_f32(&p, 1.0f, 0.25f, 50.0f, -10.0f, 10.0f);
  fm_pid_reset_f32(&p, 0.0f);
  ASSERT_EQ(f2u(fm_pid_f32(&p, 1.0f, 0.0f)), f2u(1.25f)); /* no D kick */
  for (int k = 0; k < 1000; k++) {
    (void)fm_pid_f32(&p, 100.0f, 0.0f);
  }
  ASSERT_EQ(f2u(p.i), f2u(10.0f));
  ASSERT_EQ(f2u(fm_pid_f32(&p, 0.0f, 0.0f)), f2u(10.0f));
  for (int k = 0; k < 1000; k++) {
    (void)fm_pid_f32(&p, -100.0f, 0.0f);
  }
  ASSERT_EQ(f2u(p.i), f2u(-10.0f));
  ASSERT_EQ(f2u(fm_pid_f32(&p, -100.0f, 0.0f)), f2u(-10.0f));
  ASSERT_EQ(f2u(fm_pid_f32(&p, 0.0f, 0.0f)), f2u(-10.0f));
  FM_PidF32 keep = p;
  ASSERT_TRUE(isnan(fm_pid_f32(&p, NAN, 0.0f)));
  ASSERT_TRUE(isnan(fm_pid_f32(&p, 1.0f, INFINITY)));
  ASSERT_EQ(memcmp(&keep, &p, sizeof p), 0); /* state untouched */
  /* P and D cancel: 3 (1e8 - 0.1) + (-3e8 - 0.1) is -0.4 */
  fm_pid_init_f32(&p, 3.0f, 0.0f, 1.0f, -1e9f, 1e9f);
  fm_pid_reset_f32(&p, -3e8f);
  float o = fm_pid_f32(&p, 1e8f, 0.1f);
  ASSERT_EQ(f2u(o), f2u(-0.4f));

  ErrStats ei;
  ErrStats eo;
  stats_init(&ei, "fm_pid_f32 integrator, beyond 0.5 ULP", "2^-32 sum");
  stats_init(&eo, "fm_pid_f32 output, beyond 0.5 ULP", "2^-32 sum");
  uint64_t st = 907U;
  for (int c = 0; c < 20000; c++) {
    float a = rand_f32(&st) * 64.0f;
    float b = rand_f32(&st) * 64.0f;
    fm_pid_init_f32(&p, rand_f32(&st), rand_f32(&st), rand_f32(&st),
                    a < b ? a : b, a < b ? b : a);
    fm_pid_reset_f32(&p, rand_f32(&st));
    for (int k = 0; k < 32; k++) {
      float sp = rand_f32(&st);
      float pv = (k & 1) != 0 ? sp * (1.0f + 0x1p-20f) : rand_f32(&st);
      ld i0 = (ld)p.i;
      ld prev = (ld)p.prev;
      ld isum = i0 + (ld)p.ki * (ld)sp - (ld)p.ki * (ld)pv;
      ld iabs = fabsl(i0) + fabsl((ld)p.ki * (ld)sp) + fabsl((ld)p.ki * (ld)pv);
      float got = fm_pid_f32(&p, sp, pv);
      stats_add(&ei, (double)isum,
                excess(p.i, isum, iabs, p.out_min, p.out_max));
      ld i1 = (ld)p.i;
      ld osum = i1 + (ld)p.kp * (ld)sp - (ld)p.kp * (ld)pv +
                (ld)p.kd * prev - (ld)p.kd * (ld)pv;
      ld oabs = fabsl(i1) + fabsl((ld)p.kp * (ld)sp) +
                fabsl((ld)p.kp * (ld)pv) + fabsl((ld)p.kd * prev) +
                fabsl((ld)p.kd * (ld)pv);
      stats_add(&eo, (double)osum,
                excess(got, osum, oabs, p.out_min, p.out_max));
    }
  }
  ASSERT_STATS_MAX(&ei, 1.0);
  ASSERT_STATS_MAX(&eo, 1.0);
}
