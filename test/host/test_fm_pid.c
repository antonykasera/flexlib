/**
 * @file test_fm_pid.c
 * @brief Host tests for fm_pid.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fm_pid.h"
#include "test_stats.h"
#include <stdbool.h>
#include <string.h>

__extension__ typedef __int128 i128;
typedef long double ld;

static i128 clamp128(i128 v, i128 lo, i128 hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static fm_q16 ref_q16(FM_PidQ16 *r, fm_q16 sp, fm_q16 pv) {
  i128 lo = (i128)r->out_min * 65536;
  i128 hi = (i128)r->out_max * 65536;
  i128 i0 = r->i;
  i128 i = clamp128(i0 + (i128)r->ki * sp - (i128)r->ki * pv, lo, hi);
  i128 pd = (i128)r->kp * sp - (i128)r->kp * pv + (i128)r->kd * r->prev -
            (i128)r->kd * pv;
  i128 o = clamp128(i + pd, lo, hi);
  if (o == hi && i > i0) {
    i = clamp128(hi - pd, i0, i);
  } else if (o == lo && i < i0) {
    i = clamp128(lo - pd, i, i0);
  }
  o = clamp128(i + pd, lo, hi);
  r->i = (int64_t)i;
  r->prev = pv;
  return (fm_q16)((o + 32768) >> 16);
}

static double plant_peak_q16(void) {
  FM_PidQ16 p;
  fm_pid_init_q16(&p, 3 * FM_Q16_ONE / 2, 983, 0, 0, FM_Q16_ONE);
  fm_pid_reset_q16(&p, 0);
  double v = 0.0;
  double peak = 0.0;
  for (int k = 0; k < 2000; k++) {
    fm_q16 u = fm_pid_q16(&p, 108134, (fm_q16)(v * 65536.0));
    v += (u / 65536.0 * 3.3 - v) * 0.01;
    peak = v > peak ? v : peak;
  }
  ASSERT_TRUE(fabs(v - 1.65) < 0.001);
  return peak;
}

static fm_q16 rand_q16(uint64_t *st) {
  uint32_t u = test_rng_u32(st);
  switch (u & 7U) {
  case 0:
    return INT32_MIN;
  case 1:
    return INT32_MAX;
  case 2:
    return (fm_q16)(u >> 3) >> 12;
  default:
    return (fm_q16)test_rng_u32(st);
  }
}

void test_fm_pid_q16(void) {
  FM_PidQ16 p;
  fm_pid_init_q16(&p, FM_Q16_ONE, FM_Q16_ONE / 10, 50 * FM_Q16_ONE,
                  -10 * FM_Q16_ONE, 10 * FM_Q16_ONE);
  fm_pid_reset_q16(&p, 0);
  fm_q16 o = fm_pid_q16(&p, FM_Q16_ONE, 0);
  ASSERT_EQ(o, FM_Q16_ONE + FM_Q16_ONE / 10);
  for (int k = 0; k < 1000; k++) {
    o = fm_pid_q16(&p, 100 * FM_Q16_ONE, 0);
  }
  ASSERT_EQ(o, 10 * FM_Q16_ONE);
  ASSERT_EQ(p.i, (int64_t)(FM_Q16_ONE / 10) * 65536);
  o = fm_pid_q16(&p, 0, 0);
  ASSERT_EQ(o, FM_Q16_ONE / 10);
  fm_pid_init_q16(&p, 0, FM_Q16_ONE, 0, -10 * FM_Q16_ONE, 10 * FM_Q16_ONE);
  for (int k = 0; k < 30; k++) {
    o = fm_pid_q16(&p, FM_Q16_ONE, 0);
  }
  ASSERT_EQ(o, 10 * FM_Q16_ONE);
  ASSERT_EQ(p.i, (int64_t)10 * FM_Q16_ONE * 65536);
  ASSERT_TRUE(plant_peak_q16() < 1.65 * 1.005);
  fm_pid_init_q16(&p, FM_Q16_ONE, FM_Q16_ONE / 10, 50 * FM_Q16_ONE,
                  -10 * FM_Q16_ONE, 10 * FM_Q16_ONE);
  fm_pid_reset_q16(&p, 0);
  (void)fm_pid_q16(&p, 0, 0);
  o = fm_pid_q16(&p, -FM_Q16_ONE, FM_Q16_ONE);
  ASSERT_EQ(o, -10 * FM_Q16_ONE);

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
  if (ref <= (ld)lo || ref >= (ld)hi) {
    return f2u(got) == f2u(ref <= (ld)lo ? lo : hi) ? 0.0 : HUGE_VAL;
  }
  int e = 0;
  (void)frexpl(ref, &e);
  ld ulp = ldexpl(1.0L, e - 24 < -149 ? -149 : e - 24);
  ld d = fabsl((ld)got - ref) - ulp / 2.0L;
  return d <= 0.0L ? 0.0 : (double)(d / (sabs * 0x1p-32L));
}

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
  ASSERT_EQ(f2u(fm_pid_f32(&p, 1.0f, 0.0f)), f2u(1.25f));
  for (int k = 0; k < 1000; k++) {
    (void)fm_pid_f32(&p, 100.0f, 0.0f);
  }
  ASSERT_EQ(f2u(p.i), f2u(0.25f));
  ASSERT_EQ(f2u(fm_pid_f32(&p, 0.0f, 0.0f)), f2u(0.25f));
  for (int k = 0; k < 1000; k++) {
    (void)fm_pid_f32(&p, -100.0f, 0.0f);
  }
  ASSERT_EQ(f2u(p.i), f2u(0.25f));
  ASSERT_EQ(f2u(fm_pid_f32(&p, -100.0f, 0.0f)), f2u(-10.0f));
  ASSERT_EQ(f2u(fm_pid_f32(&p, 0.0f, 0.0f)), f2u(0.25f));
  fm_pid_init_f32(&p, 0.0f, 1.0f, 0.0f, -10.0f, 10.0f);
  for (int k = 0; k < 30; k++) {
    (void)fm_pid_f32(&p, 1.0f, 0.0f);
  }
  ASSERT_EQ(f2u(p.i), f2u(10.0f));
  fm_pid_init_f32(&p, 1.5f, 0.015f, 0.0f, 0.0f, 1.0f);
  double v = 0.0;
  double peak = 0.0;
  for (int k = 0; k < 2000; k++) {
    float u = fm_pid_f32(&p, 1.65f, (float)v);
    v += ((double)u * 3.3 - v) * 0.01;
    peak = v > peak ? v : peak;
  }
  ASSERT_TRUE(fabs(v - 1.65) < 0.001 && peak < 1.65 * 1.005);
  fm_pid_init_f32(&p, 1.0f, 0.25f, 50.0f, -10.0f, 10.0f);
  FM_PidF32 keep = p;
  ASSERT_TRUE(isnan(fm_pid_f32(&p, NAN, 0.0f)));
  ASSERT_TRUE(isnan(fm_pid_f32(&p, 1.0f, INFINITY)));
  ASSERT_EQ(memcmp(&keep, &p, sizeof p), 0);
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
      ld i1 = (ld)p.i;
      ld pd = (ld)p.kp * (ld)sp - (ld)p.kp * (ld)pv + (ld)p.kd * prev -
              (ld)p.kd * (ld)pv;
      ld lo = (ld)p.out_min;
      ld hi = (ld)p.out_max;
      ld step = isum < lo ? lo : (isum > hi ? hi : isum);
      ld ilo = i0 < step ? i0 : step;
      ld ihi = i0 < step ? step : i0;
      ld tol = (fabsl(ilo) + fabsl(ihi)) * 0x1p-23L + 0x1p-140L;
      bool between = i1 >= ilo - tol && i1 <= ihi + tol;
      ld near = fabsl(hi) * 0x1p-22L + 0x1p-140L;
      bool at_limit = fabsl((ld)got - hi) <= near || fabsl((ld)got - lo) <= near;
      bool limited = (step + pd >= hi && step > i0) || (step + pd <= lo && step < i0);
      double ex = excess(p.i, isum, iabs, p.out_min, p.out_max);
      if (limited || (ex > 1.0 && at_limit)) {
        ASSERT_TRUE(between);
      } else {
        stats_add(&ei, (double)isum, ex);
      }
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
