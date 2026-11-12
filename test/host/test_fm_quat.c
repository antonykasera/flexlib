/**
 * @file test_fm_quat.c
 * @brief Host tests for fm_quat.h against long double references
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fm_quat.h"
#include "test_stats.h"
#include <string.h>

typedef long double ld;

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

/* +-[2^-20, 2^20), with zeros */
static float rand_f32(uint64_t *st, unsigned span) {
  uint32_t u = test_rng_u32(st);
  if ((u & 15U) == 0U) {
    return 0.0f;
  }
  return u2f((u & 0x807FFFFFU) | ((127U - span + (u >> 23) % (2U * span)) << 23));
}

static void quat_ld(const FM_Quat *q, ld v[4]) {
  v[0] = (ld)q->w;
  v[1] = (ld)q->x;
  v[2] = (ld)q->y;
  v[3] = (ld)q->z;
}

static void mul_ld(const ld a[4], const ld b[4], ld r[4]) {
  r[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
  r[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
  r[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
  r[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
}

/* |got - ref| beyond half an ULP, in units of 2^-32 sum |products| */
static double excess(float got, ld ref, ld sabs) {
  int e = 0;
  (void)frexpl(ref, &e);
  ld ulp = ldexpl(1.0L, e - 24 < -149 ? -149 : e - 24);
  ld d = fabsl((ld)got - ref) - ulp / 2.0L;
  return d <= 0.0L ? 0.0 : (double)(d / (sabs * 0x1p-32L));
}

static FM_Quat rand_quat(uint64_t *st, unsigned span) {
  FM_Quat q = {rand_f32(st, span), rand_f32(st, span), rand_f32(st, span),
               rand_f32(st, span)};
  return q;
}

/* q / |q| rounded to float: unit to 2^-24 */
static FM_Quat unit_quat(uint64_t *st) {
  FM_Quat q = rand_quat(st, 3U);
  ld v[4];
  quat_ld(&q, v);
  ld n = sqrtl(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
  if (n == 0.0L) {
    FM_Quat one = {1.0f, 0.0f, 0.0f, 0.0f};
    return one;
  }
  FM_Quat u = {(float)(v[0] / n), (float)(v[1] / n), (float)(v[2] / n),
               (float)(v[3] / n)};
  return u;
}

static void comps(const FM_Quat *q, float c[4]) { memcpy(c, q, 16); }

void test_fm_quat(void) {
  /* specials, identity, aliasing */
  FM_Quat z = {0.0f, 0.0f, 0.0f, 0.0f};
  FM_Quat r;
  fm_quat_normalize(&r, &z);
  ASSERT_EQ(f2u(r.w), f2u(1.0f));
  ASSERT_EQ(f2u(r.x), 0U);
  FM_Quat bad = {1.0f, INFINITY, 0.0f, 0.0f};
  FM_Quat one = {1.0f, 0.0f, 0.0f, 0.0f};
  fm_quat_mul(&r, &one, &bad);
  ASSERT_TRUE(isnan(r.w) && isnan(r.z));
  fm_quat_normalize(&r, &bad);
  ASSERT_TRUE(isnan(r.x));
  const float w0[3] = {0.0f, 0.0f, 0.0f};
  const float wn[3] = {0.0f, NAN, 0.0f};
  FM_Quat h = {2.0f, 0.0f, 0.0f, 0.0f};
  fm_quat_integrate(&r, &h, w0, 0.01f);
  ASSERT_EQ(f2u(r.w), f2u(1.0f)); /* w = 0: normalised */
  fm_quat_integrate(&r, &one, wn, 0.01f);
  ASSERT_TRUE(isnan(r.w));
  const float w1[3] = {1.0f, 0.0f, 0.0f};
  fm_quat_integrate(&r, &one, w1, NAN);
  ASSERT_TRUE(isnan(r.w));
  const float wover[3] = {1e30f, 0.0f, 0.0f};
  fm_quat_integrate(&r, &one, wover, 1e-3f); /* |w|^2 overflows */
  ASSERT_TRUE(isnan(r.w));
  const float wbig[3] = {1e19f, 0.0f, 0.0f};
  fm_quat_integrate(&r, &one, wbig, 1e30f); /* |w| dt / 2 overflows */
  ASSERT_TRUE(isnan(r.w) && isnan(r.x));
  /* 10^5 steps of 1 ms: |q| stays 1 (no drift) */
  const float wd[3] = {3.1f, -1.7f, 0.4f};
  FM_Quat d = {0.5f, 0.5f, -0.5f, 0.5f};
  for (int i = 0; i < 100000; i++) {
    fm_quat_integrate(&d, &d, wd, 0.001f);
  }
  ld dn = sqrtl((ld)d.w * (ld)d.w + (ld)d.x * (ld)d.x + (ld)d.y * (ld)d.y +
                (ld)d.z * (ld)d.z);
  ASSERT_TRUE(fabsl(dn - 1.0L) < 0x1p-22L);
  FM_Quat tiny = {0x1p-140f, 0x1p-141f, 0.0f, 0.0f}; /* |q|^2 underflows */
  fm_quat_normalize(&r, &tiny);
  ASSERT_TRUE(fabsf(r.w - 0.894427191f) < 1e-6f);
  FM_Quat huge = {0x1p120f, 0.0f, 0x1p120f, 0.0f}; /* |q|^2 overflows */
  fm_quat_normalize(&r, &huge);
  ASSERT_EQ(f2u(r.w), f2u(0.707106781f));
  const float vx[3] = {1.0f, 0.0f, 0.0f};
  float rv[3];
  FM_Quat qz = {0.707106781f, 0.0f, 0.0f, 0.707106781f}; /* 90 deg about z */
  fm_quat_rotate(rv, &qz, vx);
  ASSERT_TRUE(fabsf(rv[0]) < 1e-7f && fabsf(rv[1] - 1.0f) < 1e-7f);

  ErrStats em;
  ErrStats en;
  ErrStats er;
  ErrStats ei;
  stats_init(&em, "fm_quat_mul, beyond 0.5 ULP", "2^-32 sum");
  stats_init(&en, "fm_quat_normalize, any magnitude", "ulp");
  stats_init(&er, "fm_quat_rotate, unit q", "2^-24 |v|");
  stats_init(&ei, "fm_quat_integrate, unit q, 2000 dps, dt <= 20 ms",
             "2^-24");
  uint64_t st = 701U;
  for (int t = 0; t < 200000; t++) {
    FM_Quat a = rand_quat(&st, 20U);
    FM_Quat b = rand_quat(&st, 20U);
    ld la[4];
    ld lb[4];
    ld lr[4];
    quat_ld(&a, la);
    quat_ld(&b, lb);
    mul_ld(la, lb, lr);
    fm_quat_mul(&r, &a, &b);
    float c[4];
    comps(&r, c);
    for (int i = 0; i < 4; i++) {
      ld sabs = 0.0L;
      for (int k = 0; k < 4; k++) {
        sabs += fabsl(la[k]) * fabsl(lb[k]);
      }
      stats_add(&em, (double)lr[i], excess(c[i], lr[i], sabs));
    }
    FM_Quat a2 = a; /* r aliasing a */
    fm_quat_mul(&a2, &a2, &b);
    ASSERT_EQ(memcmp(&a2, &r, sizeof r), 0);

    FM_Quat g = rand_quat(&st, 60U);
    ld lg[4];
    quat_ld(&g, lg);
    ld n = sqrtl(lg[0] * lg[0] + lg[1] * lg[1] + lg[2] * lg[2] + lg[3] * lg[3]);
    fm_quat_normalize(&r, &g);
    comps(&r, c);
    if (n != 0.0L) {
      for (int i = 0; i < 4; i++) {
        stats_add(&en, (double)n, ulp_err_f32(c[i], (double)(lg[i] / n)));
      }
    }

    FM_Quat u = unit_quat(&st);
    ld lu[4];
    quat_ld(&u, lu);
    float v[3] = {rand_f32(&st, 10U), rand_f32(&st, 10U), rand_f32(&st, 10U)};
    ld lv[3] = {(ld)v[0], (ld)v[1], (ld)v[2]};
    ld tv[3];
    ld ref[3];
    for (int i = 0; i < 3; i++) {
      int j = (i + 1) % 3;
      int k = (i + 2) % 3;
      tv[i] = lu[1 + j] * lv[k] - lu[1 + k] * lv[j];
    }
    for (int i = 0; i < 3; i++) {
      int j = (i + 1) % 3;
      int k = (i + 2) % 3;
      ref[i] = lv[i] + 2.0L * lu[0] * tv[i] +
               2.0L * (lu[1 + j] * tv[k] - lu[1 + k] * tv[j]);
    }
    fm_quat_rotate(rv, &u, v);
    ld vn = sqrtl(lv[0] * lv[0] + lv[1] * lv[1] + lv[2] * lv[2]);
    for (int i = 0; i < 3; i++) {
      if (vn != 0.0L) {
        stats_add(&er, (double)vn, (double)(((ld)rv[i] - ref[i]) / vn * 0x1p24L));
      }
    }

    /* +-35 rad/s, dt 0.1 .. 20 ms */
    float w[3];
    for (int i = 0; i < 3; i++) {
      w[i] = (float)((int32_t)test_rng_u32(&st)) * 0x1p-31f * 35.0f;
    }
    float dt = (float)(test_rng_u32(&st) % 200U + 1U) * 1e-4f;
    ld lw[3] = {(ld)w[0], (ld)w[1], (ld)w[2]};
    ld wm = sqrtl(lw[0] * lw[0] + lw[1] * lw[1] + lw[2] * lw[2]);
    ld th = wm * (ld)dt / 2.0L;
    ld dq[4] = {cosl(th), sinl(th) * lw[0] / wm, sinl(th) * lw[1] / wm,
                sinl(th) * lw[2] / wm};
    ld out[4];
    mul_ld(lu, dq, out);
    ld on = sqrtl(out[0] * out[0] + out[1] * out[1] + out[2] * out[2] +
                  out[3] * out[3]);
    fm_quat_integrate(&r, &u, w, dt);
    comps(&r, c);
    for (int i = 0; i < 4; i++) {
      stats_add(&ei, (double)th, (double)(((ld)c[i] - out[i] / on) * 0x1p24L));
    }
  }
  ASSERT_STATS_MAX(&em, 1.0);
  ASSERT_STATS_MAX(&en, 0.6);
  ASSERT_STATS_MAX(&er, 4.0);
  ASSERT_STATS_MAX(&ei, 4.0);
}
