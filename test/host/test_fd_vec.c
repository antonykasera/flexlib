/**
 * @file test_fd_vec.c
 * @brief Host tests for fd_vec.h against exact 128-bit references
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_vec.h"
#include "fd_wide.h"
#include "test_stats.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

__extension__ typedef __int128 i128;
__extension__ typedef unsigned __int128 u128;

#define CAP 300U
#define CANARY 0x5AU

static const uint32_t lens[] = {0U, 1U,  2U,  3U,  4U,  5U,  7U,
                                8U, 15U, 16U, 17U, 63U, 64U, 255U};
#define NLENS (sizeof lens / sizeof lens[0])

static const int32_t edge15[] = {-32768, -32767, -16384, -2,    -1,   0,
                                 1,      2,      16383,  32766, 32767};
static const int32_t edge31[] = {INT32_MIN, INT32_MIN + 1, -65536,   -1, 0, 1,
                                 65535,     INT32_MAX - 1, INT32_MAX};

static uint64_t rng = 2026U;

static fm_q15 rnd15(void) {
  uint32_t u = test_rng_u32(&rng);
  if ((u & 7U) == 0U) {
    return (fm_q15)edge15[(u >> 3) % (sizeof edge15 / sizeof edge15[0])];
  }
  return (fm_q15)(u >> 16);
}

static fm_q31 rnd31(void) {
  uint32_t u = test_rng_u32(&rng);
  if ((u & 7U) == 0U) {
    return edge31[(u >> 3) % (sizeof edge31 / sizeof edge31[0])];
  }
  return (fm_q31)test_rng_u32(&rng) >> (u & 15U);
}

static void fill15(fm_q15 *v, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    v[i] = rnd15();
  }
}

static void fill31(fm_q31 *v, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    v[i] = rnd31();
  }
}

/* round(v / 2^s), ties up */
static i128 rdiv(i128 v, unsigned s) {
  return s == 0U ? v : (v + ((i128)1 << (s - 1U))) >> s;
}

static i128 clampi(i128 v, i128 lo, i128 hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

#define C15(v) (int32_t)clampi((v), -32768, 32767)
#define C31(v) (int32_t)clampi((v), INT32_MIN, INT32_MAX)

static bool canary_ok(const void *p, size_t bytes) {
  const uint8_t *b = (const uint8_t *)p;
  for (size_t i = 0U; i < 16U; i++) {
    if (b[bytes + i] != CANARY) {
      return false;
    }
  }
  return true;
}

/* ---- element-wise: out of place, then in place ---- */

typedef void (*Bin15)(const fm_q15 *, const fm_q15 *, fm_q15 *, uint32_t);
typedef void (*Bin31)(const fm_q31 *, const fm_q31 *, fm_q31 *, uint32_t);

static int32_t r_add(int64_t a, int64_t b, int bits) {
  return bits == 15 ? C15((i128)a + b) : C31((i128)a + b);
}
static int32_t r_sub(int64_t a, int64_t b, int bits) {
  return bits == 15 ? C15((i128)a - b) : C31((i128)a - b);
}
static int32_t r_mul(int64_t a, int64_t b, int bits) {
  i128 p = rdiv((i128)a * b, (unsigned)bits);
  return bits == 15 ? C15(p) : C31(p);
}

static void check_bin15(Bin15 f, int32_t (*ref)(int64_t, int64_t, int)) {
  static fm_q15 a[CAP], b[CAP], y[CAP + 16U];
  for (unsigned l = 0U; l < NLENS; l++) {
    uint32_t n = lens[l];
    fill15(a, n);
    fill15(b, n);
    memset(y, CANARY, sizeof y);
    f(a, b, y, n);
    for (uint32_t i = 0U; i < n; i++) {
      ASSERT_EQ(y[i], ref(a[i], b[i], 15));
    }
    ASSERT_TRUE(canary_ok(y, n * sizeof y[0]));
    memcpy(y, a, n * sizeof y[0]);
    f(y, b, y, n);
    for (uint32_t i = 0U; i < n; i++) {
      ASSERT_EQ(y[i], ref(a[i], b[i], 15));
    }
  }
}

static void check_bin31(Bin31 f, int32_t (*ref)(int64_t, int64_t, int)) {
  static fm_q31 a[CAP], b[CAP], y[CAP + 4U];
  for (unsigned l = 0U; l < NLENS; l++) {
    uint32_t n = lens[l];
    fill31(a, n);
    fill31(b, n);
    memset(y, CANARY, sizeof y);
    f(a, b, y, n);
    for (uint32_t i = 0U; i < n; i++) {
      ASSERT_EQ(y[i], ref(a[i], b[i], 31));
    }
    ASSERT_TRUE(canary_ok(y, n * sizeof y[0]));
    memcpy(y, b, n * sizeof y[0]);
    f(a, y, y, n);
    for (uint32_t i = 0U; i < n; i++) {
      ASSERT_EQ(y[i], ref(a[i], b[i], 31));
    }
  }
}

void test_fd_vec_binary(void) {
  for (int rep = 0; rep < 20; rep++) {
    check_bin15(fd_add_q15, r_add);
    check_bin15(fd_sub_q15, r_sub);
    check_bin15(fd_mul_q15, r_mul);
    check_bin31(fd_add_q31, r_add);
    check_bin31(fd_sub_q31, r_sub);
    check_bin31(fd_mul_q31, r_mul);
  }
}

void test_fd_vec_unary(void) {
  static fm_q15 x[CAP], y[CAP + 16U];
  static fm_q31 X[CAP], Y[CAP + 4U];
  for (int rep = 0; rep < 20; rep++) {
    for (unsigned l = 0U; l < NLENS; l++) {
      uint32_t n = lens[l];
      fill15(x, n);
      fill31(X, n);

      for (int32_t s = -15; s <= 15; s++) {
        fm_q15 k = rnd15();
        memset(y, CANARY, sizeof y);
        fd_scale_q15(x, k, s, y, n);
        for (uint32_t i = 0U; i < n; i++) {
          ASSERT_EQ(y[i],
                    C15(rdiv((i128)x[i] * k * ((i128)1 << (s + 16)), 31U)));
        }
        ASSERT_TRUE(canary_ok(y, n * 2U));
        fd_shift_q15(x, s, y, n);
        for (uint32_t i = 0U; i < n; i++) {
          i128 v =
              s >= 0 ? (i128)x[i] * ((i128)1 << s) : rdiv(x[i], (unsigned)-s);
          ASSERT_EQ(y[i], C15(v));
        }
      }
      for (int32_t s = -31; s <= 31; s++) {
        fm_q31 k = rnd31();
        memset(Y, CANARY, sizeof Y);
        fd_scale_q31(X, k, s, Y, n);
        for (uint32_t i = 0U; i < n; i++) {
          ASSERT_EQ(Y[i],
                    C31(rdiv((i128)X[i] * k * ((i128)1 << (s + 32)), 63U)));
        }
        ASSERT_TRUE(canary_ok(Y, n * 4U));
        fd_shift_q31(X, s, Y, n);
        for (uint32_t i = 0U; i < n; i++) {
          i128 v =
              s >= 0 ? (i128)X[i] * ((i128)1 << s) : rdiv(X[i], (unsigned)-s);
          ASSERT_EQ(Y[i], C31(v));
        }
      }

      fm_q15 c = rnd15();
      fm_q31 C = rnd31();
      fd_offset_q15(x, c, y, n);
      fd_offset_q31(X, C, Y, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(y[i], C15((i128)x[i] + c));
        ASSERT_EQ(Y[i], C31((i128)X[i] + C));
      }
      fd_negate_q15(x, y, n);
      fd_negate_q31(X, Y, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(y[i], C15(-(i128)x[i]));
        ASSERT_EQ(Y[i], C31(-(i128)X[i]));
      }
      fd_abs_q15(x, y, n);
      fd_abs_q31(X, Y, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(y[i], C15(x[i] < 0 ? -(i128)x[i] : x[i]));
        ASSERT_EQ(Y[i], C31(X[i] < 0 ? -(i128)X[i] : X[i]));
      }
      fm_q15 lo = rnd15(), hi = rnd15();
      if (lo > hi) {
        fm_q15 t = lo;
        lo = hi;
        hi = t;
      }
      fd_clip_q15(x, lo, hi, y, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(y[i], (int32_t)clampi(x[i], lo, hi));
      }
      fm_q31 Lo = -(fm_q31)(test_rng_u32(&rng) >> 1), Hi = (fm_q31)rnd31();
      Hi = Hi < Lo ? Lo : Hi;
      fd_clip_q31(X, Lo, Hi, Y, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(Y[i], (int32_t)clampi(X[i], Lo, Hi));
      }

      memset(y, CANARY, sizeof y);
      memset(Y, CANARY, sizeof Y);
      fd_fill_q15(c, y, n);
      fd_fill_q31(C, Y, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(y[i], c);
        ASSERT_EQ(Y[i], C);
      }
      ASSERT_TRUE(canary_ok(y, n * 2U) && canary_ok(Y, n * 4U));
      fd_copy_q15(x, y, n);
      fd_copy_q31(X, Y, n);
      ASSERT_TRUE(memcmp(x, y, n * 2U) == 0 && memcmp(X, Y, n * 4U) == 0);
      ASSERT_TRUE(canary_ok(y, n * 2U) && canary_ok(Y, n * 4U));
      fd_negate_q15(y, y, n); /* in place */
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(y[i], C15(-(i128)x[i]));
      }
    }
  }
}

void test_fd_vec_convert(void) {
  static fm_q15 h[CAP], h2[CAP];
  static fm_q31 w[CAP];
  static int8_t b[CAP];
  static float f[CAP];
  for (int32_t v = -32768; v <= 32767; v++) {
    fm_q15 x = (fm_q15)v;
    int8_t q7;
    fd_q15_to_q7(&x, &q7, 1U);
    ASSERT_EQ(q7, (int32_t)clampi(rdiv(v, 8U), -128, 127));
    fm_q31 q31;
    fd_q15_to_q31(&x, &q31, 1U);
    ASSERT_EQ(q31, (int64_t)v * 65536);
  }
  for (int32_t v = -128; v <= 127; v++) {
    int8_t x = (int8_t)v;
    fm_q15 q;
    fd_q7_to_q15(&x, &q, 1U);
    ASSERT_EQ(q, v * 256);
  }
  for (int rep = 0; rep < 50; rep++) {
    for (unsigned l = 0U; l < NLENS; l++) {
      uint32_t n = lens[l];
      fill31(w, n);
      fd_q31_to_q15(w, h, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(h[i], C15(rdiv(w[i], 16U)));
      }
      fd_q31_to_f32(w, f, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_TRUE(f[i] == fm_q31_to_f32(w[i]));
      }
      fd_f32_to_q31(f, w, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(w[i], fm_f32_to_q31(f[i]));
      }
      fill15(h, n);
      fd_q15_to_f32(h, f, n);
      fd_f32_to_q15(f, h2, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_TRUE(f[i] == (float)h[i] / 32768.0f);
        ASSERT_EQ(h2[i], h[i]);
      }
      fd_q15_to_q7(h, b, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(b[i], (int32_t)clampi(rdiv(h[i], 8U), -128, 127));
      }
    }
  }
}

void test_fd_vec_db(void) {
  ErrStats s15, s31;
  stats_init(&s15, "fd_db_q15", "lsb");
  stats_init(&s31, "fd_db_q31", "lsb");
  for (int32_t v = -2; v <= 32767; v++) {
    fm_q15 x = (fm_q15)v;
    fm_q16 d;
    fd_db_q15(&x, &d, 1U);
    if (v <= 0) {
      ASSERT_EQ(d, INT32_MIN);
      continue;
    }
    stats_add(&s15, v, d - 20.0 * log10(v / 32768.0) * 65536.0);
  }
  fm_q31 neg[2] = {0, INT32_MIN};
  fm_q16 dn[2];
  fd_db_q31(neg, dn, 2U);
  ASSERT_EQ(dn[0], INT32_MIN);
  ASSERT_EQ(dn[1], INT32_MIN);
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&rng);
    fm_q31 x = (fm_q31)((u >> 1) >> (u & 31U));
    if (x <= 0) {
      continue;
    }
    fm_q16 d;
    fd_db_q31(&x, &d, 1U);
    stats_add(&s31, x, d - 20.0 * log10(x / 2147483648.0) * 65536.0);
  }
  ASSERT_STATS_MAX(&s15, 0.54);
  ASSERT_STATS_MAX(&s31, 0.97);
}

/* ---- statistics ---- */

/* round(sqrt(num / d)) exactly, from a long double guess */
static uint64_t ref_sqrt_round(u128 num, u128 d) {
  long double g = sqrtl((long double)num / (long double)d);
  uint64_t k = (uint64_t)llroundl(g);
  for (;;) { /* (2k - 1)^2 d <= 4 num < (2k + 1)^2 d */
    u128 lo = k == 0U ? 0U : (u128)(2U * k - 1U) * (2U * k - 1U) * d;
    u128 hi = (u128)(2U * k + 1U) * (2U * k + 1U) * d;
    if (4U * num < lo) {
      k--;
    } else if (4U * num >= hi) {
      k++;
    } else {
      return k;
    }
  }
}

static i128 floor_div(i128 a, i128 b) {
  i128 q = a / b;
  return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

static void check_stats15(const fm_q15 *x, uint32_t n) {
  i128 s1 = 0;
  u128 s2 = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    s1 += x[i];
    s2 = s2 + (uint64_t)((int64_t)x[i] * x[i]);
  }
  ASSERT_TRUE(fd_sum_q15(x, n) == (int64_t)s1);
  ASSERT_TRUE(fd_power_q15(x, n) == (uint64_t)s2);
  if (n == 0U) {
    return;
  }
  ASSERT_EQ(fd_mean_q15(x, n), (int64_t)floor_div(2 * s1 + n, 2 * (i128)n));
  ASSERT_EQ(fd_rms_q15(x, n), (int64_t)clampi(ref_sqrt_round(s2, n), 0, 32767));
  if (n >= 2U) {
    u128 num = (u128)n * s2 - (u128)(s1 * s1);
    u128 d = (u128)n * (n - 1U);
    i128 var = (i128)((2U * num + (d << 15)) / (d << 16));
    ASSERT_EQ(fd_var_q15(x, n), (int64_t)clampi(var, 0, 32767));
    ASSERT_EQ(fd_std_q15(x, n),
              (int64_t)clampi(ref_sqrt_round(num, d), 0, 32767));
  }
  uint32_t im, ix, ia;
  fm_q15 mn = fd_min_q15(x, n, &im), mx = fd_max_q15(x, n, &ix);
  fm_q15 am = fd_absmax_q15(x, n, &ia);
  uint32_t jm = 0U, jx = 0U, ja = 0U;
  for (uint32_t i = 1U; i < n; i++) {
    jm = x[i] < x[jm] ? i : jm;
    jx = x[i] > x[jx] ? i : jx;
    ja = abs(x[i]) > abs(x[ja]) ? i : ja;
  }
  ASSERT_TRUE(mn == x[jm] && im == jm && mx == x[jx] && ix == jx);
  ASSERT_TRUE(ia == ja && am == C15(abs(x[ja])));
  fm_q15 a, b;
  fd_minmax_q15(x, n, &a, &b);
  ASSERT_TRUE(a == mn && b == mx);
  ASSERT_EQ(fd_max_q15(x, n, NULL), mx);
}

static void check_stats31(const fm_q31 *x, uint32_t n) {
  i128 s1 = 0;
  u128 s2 = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    s1 += x[i];
    s2 = s2 + (uint64_t)((int64_t)x[i] * x[i]);
  }
  ASSERT_TRUE(fd_sum_q31(x, n) == (int64_t)s1);
  u128 p48 = (s2 + 8192U) >> 14;
  ASSERT_TRUE(fd_power_q31(x, n) ==
              (p48 > UINT64_MAX ? UINT64_MAX : (uint64_t)p48));
  if (n == 0U) {
    return;
  }
  ASSERT_EQ(fd_mean_q31(x, n), (int64_t)floor_div(2 * s1 + n, 2 * (i128)n));
  ASSERT_EQ(fd_rms_q31(x, n),
            (int64_t)clampi(ref_sqrt_round(s2, n), 0, INT32_MAX));
  if (n >= 2U) {
    u128 num = (u128)n * s2 - (u128)(s1 * s1);
    u128 d = (u128)n * (n - 1U);
    i128 var = (i128)((2U * num + (d << 31)) / (d << 32));
    ASSERT_EQ(fd_var_q31(x, n), (int64_t)clampi(var, 0, INT32_MAX));
    ASSERT_EQ(fd_std_q31(x, n),
              (int64_t)clampi(ref_sqrt_round(num, d), 0, INT32_MAX));
  }
  uint32_t im, ix, ia;
  fm_q31 mn = fd_min_q31(x, n, &im), mx = fd_max_q31(x, n, &ix);
  fm_q31 am = fd_absmax_q31(x, n, &ia);
  uint32_t jm = 0U, jx = 0U, ja = 0U;
  for (uint32_t i = 1U; i < n; i++) {
    jm = x[i] < x[jm] ? i : jm;
    jx = x[i] > x[jx] ? i : jx;
    ja = llabs(x[i]) > llabs(x[ja]) ? i : ja;
  }
  ASSERT_TRUE(mn == x[jm] && im == jm && mx == x[jx] && ix == jx);
  ASSERT_TRUE(ia == ja && am == C31(llabs(x[ja])));
  fm_q31 a, b;
  fd_minmax_q31(x, n, &a, &b);
  ASSERT_TRUE(a == mn && b == mx);
}

static void check_dot(const fm_q15 *a, const fm_q15 *b, const fm_q31 *A,
                      const fm_q31 *B, uint32_t n) {
  i128 s = 0, S = 0;
  for (uint32_t i = 0U; i < n; i++) {
    s += (i128)a[i] * b[i];
    S += (i128)A[i] * B[i];
  }
  ASSERT_TRUE(fd_dot_q15(a, b, n) == (int64_t)s);
  ASSERT_TRUE(fd_dot_q31(A, B, n) ==
              (int64_t)clampi(rdiv(S, 14U), INT64_MIN, INT64_MAX));
}

void test_fd_vec_stats(void) {
  static fm_q15 x[CAP], y[CAP];
  static fm_q31 X[CAP], Y[CAP];
  for (int rep = 0; rep < 200; rep++) {
    for (unsigned l = 0U; l < NLENS; l++) {
      uint32_t n = lens[l];
      fill15(x, n);
      fill15(y, n);
      fill31(X, n);
      fill31(Y, n);
      check_stats15(x, n);
      check_stats31(X, n);
      check_dot(x, y, X, Y, n);
    }
  }
  /* full scale everywhere: saturation, and the widest sums */
  for (unsigned l = 0U; l < NLENS; l++) {
    uint32_t n = lens[l];
    for (uint32_t i = 0U; i < n; i++) {
      x[i] = -32768;
      y[i] = (i & 1U) != 0U ? 32767 : -32768;
      X[i] = INT32_MIN;
      Y[i] = (i & 1U) != 0U ? INT32_MAX : INT32_MIN;
    }
    check_stats15(x, n);
    check_stats15(y, n);
    check_stats31(X, n);
    check_stats31(Y, n);
    check_dot(x, x, X, X, n);
    check_dot(x, y, X, Y, n);
  }
  /* long blocks: q31 dot saturates past 2^15, power past 2^16 */
  static fm_q15 big[70000];
  static fm_q31 BIG[70000];
  for (uint32_t i = 0U; i < 70000U; i++) {
    big[i] = -32768;
  }
  for (uint32_t i = 0U; i < 70000U; i++) {
    BIG[i] = INT32_MIN;
  }
  check_stats15(big, 70000U);
  check_stats31(BIG, 70000U);
  ASSERT_TRUE(fd_dot_q31(BIG, BIG, 40000U) == INT64_MAX);
  ASSERT_TRUE(fd_power_q31(BIG, 60000U) != UINT64_MAX);
  ASSERT_TRUE(fd_power_q31(BIG, 70000U) == UINT64_MAX);

  /* the exact saturation thresholds: 2^15 for dot, 2^16 for power (Q48) */
  static fm_q31 MAXV[40000];
  for (uint32_t i = 0U; i < 40000U; i++) {
    MAXV[i] = INT32_MAX;
  }
  ASSERT_TRUE(fd_dot_q31(BIG, BIG, 32768U) == INT64_MAX);
  ASSERT_TRUE(fd_dot_q31(BIG, BIG, 32767U) == (int64_t)32767 << 48);
  ASSERT_TRUE(fd_dot_q31(BIG, MAXV, 32768U) ==
              (int64_t)clampi(rdiv(-(i128)32768 * ((i128)1 << 62) +
                                       (i128)32768 * ((i128)1 << 31),
                                   14U),
                              INT64_MIN, INT64_MAX));
  ASSERT_TRUE(fd_dot_q31(BIG, MAXV, 32769U) == INT64_MIN);
  ASSERT_TRUE(fd_power_q31(BIG, 65536U) == UINT64_MAX);
  ASSERT_TRUE(fd_power_q31(BIG, 65535U) == (uint64_t)65535 << 48);
}

/* complex dot over 2^15 terms of the largest partial products */
void test_fd_vec_cdot_long(void) {
  static fd_cq31 a[40000], b[40000];
  for (uint32_t i = 0U; i < 40000U; i++) {
    a[i].re = INT32_MAX;
    a[i].im = INT32_MIN;
    b[i].re = 0xFFFF;
    b[i].im = (i & 1U) != 0U ? 0xFFFF : (fm_q31)0x8000FFFF;
  }
  i128 sr = 0, si = 0;
  for (uint32_t i = 0U; i < 40000U; i++) {
    sr += (i128)a[i].re * b[i].re - (i128)a[i].im * b[i].im;
    si += (i128)a[i].re * b[i].im + (i128)a[i].im * b[i].re;
  }
  int64_t re, im;
  fd_cdot_q31(a, b, 40000U, &re, &im);
  ASSERT_TRUE(re == (int64_t)clampi(rdiv(sr, 14U), INT64_MIN, INT64_MAX));
  ASSERT_TRUE(im == (int64_t)clampi(rdiv(si, 14U), INT64_MIN, INT64_MAX));
}

/* ---- complex ---- */

void test_fd_vec_complex(void) {
  static fd_cq15 a[CAP], b[CAP], y[CAP];
  static fd_cq31 A[CAP], B[CAP], Y[CAP];
  static fm_q15 r15[CAP], i15[CAP];
  static fm_q31 r31[CAP], i31[CAP];
  static uint16_t m15[CAP];
  static uint32_t m31[CAP], p15[CAP], p31[CAP];
  for (int rep = 0; rep < 100; rep++) {
    for (unsigned l = 0U; l < NLENS; l++) {
      uint32_t n = lens[l];
      for (uint32_t i = 0U; i < n; i++) {
        a[i].re = rnd15();
        a[i].im = rnd15();
        b[i].re = rnd15();
        b[i].im = rnd15();
        A[i].re = rnd31();
        A[i].im = rnd31();
        B[i].re = rnd31();
        B[i].im = rnd31();
        r15[i] = rnd15();
        r31[i] = rnd31();
      }
      if (rep == 0 && n > 0U) {
        a[0].re = a[0].im = b[0].re = b[0].im = -32768;
        A[0].re = A[0].im = B[0].re = B[0].im = INT32_MIN;
      }
      fd_cmul_q15(a, b, y, n);
      fd_cmul_q31(A, B, Y, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(
            y[i].re,
            C15(rdiv((i128)a[i].re * b[i].re - (i128)a[i].im * b[i].im, 15U)));
        ASSERT_EQ(
            y[i].im,
            C15(rdiv((i128)a[i].re * b[i].im + (i128)a[i].im * b[i].re, 15U)));
        ASSERT_EQ(
            Y[i].re,
            C31(rdiv((i128)A[i].re * B[i].re - (i128)A[i].im * B[i].im, 31U)));
        ASSERT_EQ(
            Y[i].im,
            C31(rdiv((i128)A[i].re * B[i].im + (i128)A[i].im * B[i].re, 31U)));
      }
      memcpy(y, a, n * sizeof y[0]);
      fd_cmul_q15(y, b, y, n); /* in place */
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(
            y[i].re,
            C15(rdiv((i128)a[i].re * b[i].re - (i128)a[i].im * b[i].im, 15U)));
      }
      fd_cmul_real_q15(a, r15, y, n);
      fd_cmul_real_q31(A, r31, Y, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_EQ(y[i].re, C15(rdiv((i128)a[i].re * r15[i], 15U)));
        ASSERT_EQ(y[i].im, C15(rdiv((i128)a[i].im * r15[i], 15U)));
        ASSERT_EQ(Y[i].re, C31(rdiv((i128)A[i].re * r31[i], 31U)));
        ASSERT_EQ(Y[i].im, C31(rdiv((i128)A[i].im * r31[i], 31U)));
      }
      fd_conj_q15(a, y, n);
      fd_conj_q31(A, Y, n);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_TRUE(y[i].re == a[i].re && y[i].im == C15(-(i128)a[i].im));
        ASSERT_TRUE(Y[i].re == A[i].re && Y[i].im == C31(-(i128)A[i].im));
      }
      fd_cmag_q15(a, m15, n);
      fd_cmag_q31(A, m31, n);
      fd_cmag2_q15(a, p15, n);
      fd_cmag2_q31(A, p31, n);
      for (uint32_t i = 0U; i < n; i++) {
        u128 s = (u128)((i128)a[i].re * a[i].re + (i128)a[i].im * a[i].im);
        u128 S = (u128)((i128)A[i].re * A[i].re + (i128)A[i].im * A[i].im);
        ASSERT_EQ(m15[i], (int64_t)ref_sqrt_round(s, 1U));
        ASSERT_EQ(m31[i], (int64_t)ref_sqrt_round(S, 1U));
        ASSERT_TRUE(p15[i] == (uint32_t)s);
        u128 q = (S + (1U << 30)) >> 31;
        ASSERT_TRUE(p31[i] == (q > UINT32_MAX ? UINT32_MAX : (uint32_t)q));
      }
      int64_t re, im;
      i128 sr = 0, si = 0, Sr = 0, Si = 0;
      for (uint32_t i = 0U; i < n; i++) {
        sr += (i128)a[i].re * b[i].re - (i128)a[i].im * b[i].im;
        si += (i128)a[i].re * b[i].im + (i128)a[i].im * b[i].re;
        Sr += (i128)A[i].re * B[i].re - (i128)A[i].im * B[i].im;
        Si += (i128)A[i].re * B[i].im + (i128)A[i].im * B[i].re;
      }
      fd_cdot_q15(a, b, n, &re, &im);
      ASSERT_TRUE(re == (int64_t)sr && im == (int64_t)si);
      fd_cdot_q31(A, B, n, &re, &im);
      ASSERT_TRUE(re == (int64_t)clampi(rdiv(Sr, 14U), INT64_MIN, INT64_MAX));
      ASSERT_TRUE(im == (int64_t)clampi(rdiv(Si, 14U), INT64_MIN, INT64_MAX));

      fd_deinterleave_q15(a, r15, i15, n);
      fd_interleave_q15(r15, i15, y, n);
      fd_deinterleave_q31(A, r31, i31, n);
      fd_interleave_q31(r31, i31, Y, n);
      ASSERT_TRUE(memcmp(a, y, n * sizeof a[0]) == 0);
      ASSERT_TRUE(memcmp(A, Y, n * sizeof A[0]) == 0);
      for (uint32_t i = 0U; i < n; i++) {
        ASSERT_TRUE(r15[i] == a[i].re && i15[i] == a[i].im);
      }
    }
  }
}

void test_fd_vec_wide(void) {
  static const uint32_t dv[] = {
      1U,     2U,          3U,          255U,        65535U,     65536U,
      65537U, 0x7FFFFFFFU, 0x80000000U, 0xFFFFFFFEU, 0xFFFFFFFFU};
  for (int i = 0; i < 2000000; i++) {
    uint32_t v = (i & 3) == 0
                     ? dv[(uint32_t)i % (sizeof dv / sizeof dv[0])]
                     : test_rng_u32(&rng) >> (test_rng_u32(&rng) & 31U);
    v = v == 0U ? 1U : v;
    uint32_t u1 = (i & 7) == 1 ? v - 1U : test_rng_u32(&rng) % v;
    uint32_t u0 = (i & 15) == 2 ? 0xFFFFFFFFU : test_rng_u32(&rng);
    uint64_t u = ((uint64_t)u1 << 32) | u0;
    uint32_t r;
    uint32_t q = fd_divlu(u1, u0, v, &r);
    ASSERT_TRUE(q == u / v && r == u % v);

    u128 a = ((u128)test_rng_next(&rng) << 64) | test_rng_next(&rng);
    a >>= test_rng_u32(&rng) & 127U;
    FD_U128 w = fd_u128((uint64_t)(a >> 64), (uint64_t)a);
    uint32_t rem = fd_u128_divmod32(&w, v);
    u128 got = ((u128)w.hi << 64) | w.lo;
    ASSERT_TRUE(got == a / v && rem == (uint32_t)(a % v));
  }
}
