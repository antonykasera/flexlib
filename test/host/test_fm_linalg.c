/**
 * @file test_fm_linalg.c
 * @brief Host tests for fm_linalg.h: q31 bit for bit against exact 128-bit
 *        sums, float against long double sums of exact products
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_linalg.h"
#include "test_stats.h"
#include <float.h>
#include <string.h>

__extension__ typedef __int128 i128;

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

/* ---- q31 ---- */

/* round(sum / 2^31), ties up, saturated; product i negated if bit i of neg */
static fm_q31 ref_q31(const fm_q31 *a, const fm_q31 *b, int bs, int n,
                      unsigned neg) {
  i128 s = 0;
  for (int i = 0; i < n; i++) {
    i128 p = (i128)a[i] * b[i * bs];
    s += ((neg >> i) & 1U) != 0U ? -p : p;
  }
  i128 r = (s + ((i128)1 << 30)) >> 31;
  return r > INT32_MAX ? INT32_MAX : (r < INT32_MIN ? INT32_MIN : (fm_q31)r);
}

/* full scale, zero and +-1 often, so sums reach 4 and ties occur */
static fm_q31 rand_q31(uint64_t *st) {
  uint32_t u = test_rng_u32(st);
  switch (u & 15U) {
  case 0:
  case 1:
    return INT32_MIN;
  case 2:
  case 3:
    return INT32_MAX;
  case 4:
    return 0;
  case 5:
    return (u & 16U) != 0U ? 1 : -1;
  case 6:
    return (fm_q31)(u >> 4) - (1 << 27); /* small */
  default:
    return (fm_q31)test_rng_u32(st);
  }
}

static void check_mat_q31(int n, int bc, const fm_q31 *a, const fm_q31 *b,
                          const fm_q31 *r) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < bc; j++) {
      ASSERT_EQ(r[i * bc + j], ref_q31(a + i * n, b + j, bc, n, 0U));
    }
  }
}

void test_fm_linalg_q31(void) {
  const fm_q31 mn[4] = {INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN};
  const fm_q31 mx[4] = {INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX};
  ASSERT_EQ(fm_dot4_q31(mn, mn), INT32_MAX); /* the sum is 4: wraps in int64 */
  ASSERT_EQ(fm_dot4_q31(mn, mx), INT32_MIN);
  ASSERT_EQ(fm_dot2_q31(mn, mn), INT32_MAX); /* 2: exactly 2^63 */
  const fm_q31 c[2] = {INT32_MIN, INT32_MAX};
  ASSERT_EQ(fm_cross2_q31(c, mn), INT32_MAX); /* 2 - 2^-31 */
  ASSERT_EQ(fm_cross2_q31(mn, mx), 0);
  const fm_q31 h[2] = {1 << 30, 0};
  const fm_q31 one[2] = {1, 0};
  ASSERT_EQ(fm_dot2_q31(h, one), 1); /* 1/2 LSB: ties up */
  const fm_q31 nh[2] = {-(1 << 30), 0};
  ASSERT_EQ(fm_dot2_q31(nh, one), 0);
  uint64_t st = 503U;
  for (int k = 0; k < 300000; k++) {
    fm_q31 a[16];
    fm_q31 b[16];
    fm_q31 r[16];
    for (int i = 0; i < 16; i++) {
      a[i] = rand_q31(&st);
      b[i] = rand_q31(&st);
    }
    ASSERT_EQ(fm_dot2_q31(a, b), ref_q31(a, b, 1, 2, 0U));
    ASSERT_EQ(fm_dot3_q31(a, b), ref_q31(a, b, 1, 3, 0U));
    ASSERT_EQ(fm_dot4_q31(a, b), ref_q31(a, b, 1, 4, 0U));
    const fm_q31 x[2] = {a[0], a[1]};
    const fm_q31 y[2] = {b[1], b[0]};
    ASSERT_EQ(fm_cross2_q31(a, b), ref_q31(x, y, 1, 2, 2U));
    fm_cross3_q31(r, a, b);
    for (int i = 0; i < 3; i++) {
      const fm_q31 u[2] = {a[(i + 1) % 3], a[(i + 2) % 3]};
      const fm_q31 v[2] = {b[(i + 2) % 3], b[(i + 1) % 3]};
      ASSERT_EQ(r[i], ref_q31(u, v, 1, 2, 2U));
    }
    if (k % 8 == 0) {
      fm_mat3_mul_q31(r, a, b);
      check_mat_q31(3, 3, a, b, r);
      fm_mat3_vec_q31(r, a, b);
      check_mat_q31(3, 1, a, b, r);
      fm_mat4_mul_q31(r, a, b);
      check_mat_q31(4, 4, a, b, r);
      fm_mat4_vec_q31(r, a, b);
      check_mat_q31(4, 1, a, b, r);
    }
  }
}

/* ---- float ---- */

/* |got - ref| beyond half an ULP of ref, in units of 2^-32 sum |a_i b_i| (the
 * contract is <= 1); ref past the float range must give inf */
static double excess(float got, long double ref, long double sabs) {
  if (fabsl(ref) >= (long double)FLT_MAX * (1.0L + 0x1p-25L)) {
    return isinf(got) && (got > 0.0f) == (ref > 0.0L) ? 0.0 : HUGE_VAL;
  }
  if (isinf(got) || isnan(got)) {
    return HUGE_VAL;
  }
  int e = 0;
  (void)frexpl(ref, &e);
  long double ulp = ldexpl(1.0L, e - 24 < -149 ? -149 : e - 24);
  long double d = fabsl((long double)got - ref) - ulp / 2.0L;
  if (d <= 0.0L) {
    return 0.0;
  }
  return sabs > 0.0L ? (double)(d / (sabs * 0x1p-32L)) : HUGE_VAL;
}

/* exponents within +-24, with zeros, subnormals, and cancelling pairs */
static float rand_f32(uint64_t *st) {
  uint32_t u = test_rng_u32(st);
  switch (u & 31U) {
  case 0:
  case 1:
    return 0.0f;
  case 2:
    return u2f((u & 0x80000000U) | ((u >> 5) & 0x7FFFFFU)); /* subnormal */
  case 3:
    return u2f(test_rng_u32(st) & 0xFF7FFFFFU); /* any finite */
  default:
    return u2f((u & 0x807FFFFFU) | ((103U + (u >> 23) % 48U) << 23));
  }
}

typedef struct {
  ErrStats ulp;
  double worst; /* largest excess */
} FStats;

static void fcheck(FStats *s, float got, const float *a, const float *b,
                   int bs, int n, unsigned neg) {
  long double ref = 0.0L;
  long double sabs = 0.0L;
  for (int i = 0; i < n; i++) {
    long double p = (long double)a[i] * (long double)b[i * bs];
    ref += ((neg >> i) & 1U) != 0U ? -p : p;
    sabs += fabsl(p);
  }
  double x = excess(got, ref, sabs);
  s->worst = x > s->worst ? x : s->worst;
  if (ref != 0.0L && fabsl(ref) < (long double)FLT_MAX) {
    stats_add(&s->ulp, (double)ref, ulp_err_f32(got, (double)ref));
  }
}

void test_fm_linalg_f32(void) {
  const float z[2] = {0.0f, -0.0f};
  const float nz[2] = {-0.0f, 1.0f};
  ASSERT_EQ(f2u(fm_dot2_f32(z, nz)), 0x00000000U); /* zero sums are +0 */
  const float p[2] = {1.0f + 0x1p-23f, 1.0f};
  const float q[2] = {1.0f - 0x1p-23f, -1.0f};
  ASSERT_EQ(f2u(fm_dot2_f32(p, q)), f2u(-0x1p-46f)); /* exact cancellation */
  const float big[2] = {0x1p100f, 0x1p100f};
  ASSERT_EQ(f2u(fm_dot2_f32(big, big)), 0x7F800000U);
  const float tiny[2] = {0x1p-75f, 0x1p-75f};
  ASSERT_EQ(f2u(fm_dot2_f32(tiny, tiny)), 0x00000001U); /* 2^-149 */
  const float inf[2] = {INFINITY, 1.0f};
  const float zi[2] = {0.0f, 1.0f};
  const float ni[2] = {1.0f, -INFINITY};
  const float m1[2] = {-1.0f, 1.0f};
  const float one[2] = {1.0f, 1.0f};
  ASSERT_EQ(f2u(fm_dot2_f32(inf, one)), 0x7F800000U);
  ASSERT_EQ(f2u(fm_dot2_f32(inf, zi)), 0x7FC00000U); /* inf x 0 */
  ASSERT_EQ(f2u(fm_dot2_f32(inf, m1)), 0xFF800000U);
  ASSERT_EQ(f2u(fm_dot2_f32(inf, ni)), 0x7FC00000U); /* inf - inf */
  ASSERT_EQ(f2u(fm_cross2_f32(inf, inf)), 0x7FC00000U); /* inf - inf */
  const float nan[2] = {1.0f, NAN};
  ASSERT_TRUE(isnan(fm_dot2_f32(nan, one)));
  float m[16];
  float r[16];
  for (int i = 0; i < 16; i++) {
    m[i] = (float)i;
  }
  m[5] = INFINITY;
  const float e0[4] = {1.0f, 0.0f, 0.0f, 0.0f};
  fm_mat4_vec_f32(r, m, e0);
  ASSERT_EQ(f2u(r[1]), 0x7FC00000U); /* inf x 0 in row 1 */
  ASSERT_EQ(f2u(r[2]), f2u(8.0f));

  FStats d2 = {.worst = 0.0};
  FStats d3 = {.worst = 0.0};
  FStats d4 = {.worst = 0.0};
  FStats cr = {.worst = 0.0};
  FStats mt = {.worst = 0.0};
  stats_init(&d2.ulp, "fm_dot2_f32, random", "ulp");
  stats_init(&d3.ulp, "fm_dot3_f32, random", "ulp");
  stats_init(&d4.ulp, "fm_dot4_f32, random", "ulp");
  stats_init(&cr.ulp, "fm_cross2/3_f32, random + near-cancelling", "ulp");
  stats_init(&mt.ulp, "fm_mat3/4 mul/vec_f32, random", "ulp");
  uint64_t st = 601U;
  for (int k = 0; k < 300000; k++) {
    float a[16];
    float b[16];
    for (int i = 0; i < 16; i++) {
      a[i] = rand_f32(&st);
      b[i] = rand_f32(&st);
    }
    if ((k & 1) != 0 && a[0] != 0.0f) { /* a0 b1 close to a1 b0 */
      float c = (float)((double)a[1] * (double)b[0] / (double)a[0]);
      b[1] = isfinite(c) ? c : b[1];
    }
    fcheck(&d2, fm_dot2_f32(a, b), a, b, 1, 2, 0U);
    fcheck(&d3, fm_dot3_f32(a, b), a, b, 1, 3, 0U);
    fcheck(&d4, fm_dot4_f32(a, b), a, b, 1, 4, 0U);
    const float x[2] = {a[0], a[1]};
    const float y[2] = {b[1], b[0]};
    fcheck(&cr, fm_cross2_f32(a, b), x, y, 1, 2, 2U);
    fm_cross3_f32(r, a, b);
    for (int i = 0; i < 3; i++) {
      const float u[2] = {a[(i + 1) % 3], a[(i + 2) % 3]};
      const float v[2] = {b[(i + 2) % 3], b[(i + 1) % 3]};
      fcheck(&cr, r[i], u, v, 1, 2, 2U);
    }
    if (k % 8 == 0) {
      fm_mat3_mul_f32(r, a, b);
      for (int i = 0; i < 9; i++) {
        fcheck(&mt, r[i], a + i / 3 * 3, b + i % 3, 3, 3, 0U);
      }
      fm_mat3_vec_f32(r, a, b);
      for (int i = 0; i < 3; i++) {
        fcheck(&mt, r[i], a + i * 3, b, 1, 3, 0U);
      }
      fm_mat4_mul_f32(r, a, b);
      for (int i = 0; i < 16; i++) {
        fcheck(&mt, r[i], a + i / 4 * 4, b + i % 4, 4, 4, 0U);
      }
      fm_mat4_vec_f32(r, a, b);
      for (int i = 0; i < 4; i++) {
        fcheck(&mt, r[i], a + i * 4, b, 1, 4, 0U);
      }
    }
  }
  FStats *all[5] = {&d2, &d3, &d4, &cr, &mt};
  for (int i = 0; i < 5; i++) {
    stats_print(&all[i]->ulp);
    printf("  beyond 0.5 ULP: max %.3g x 2^-32 sum |a_i b_i|\n", all[i]->worst);
    ASSERT_TRUE(all[i]->worst <= 1.0);
  }
}
