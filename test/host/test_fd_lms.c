/**
 * @file test_fd_lms.c
 * @brief Host tests for fd_lms.h against the textbook recursion in integers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_lms.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

__extension__ typedef __int128 i128;

#define MAXT 40U
#define NS 700U

static uint64_t rng = 99U;

static int32_t sat(int64_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : (int32_t)v);
}

/* round(a / b) to nearest, ties up, b > 0 */
static int64_t rdiv64(i128 a, i128 b) {
  i128 t = 2 * a + b, q = t / (2 * b);
  return (int64_t)(q * 2 * b > t ? q - 1 : q);
}

/* one sample at a time: output with the current weights, then the update */
typedef struct {
  int32_t w[MAXT];
  int32_t hist[MAXT + 1U]; /* hist[k] = x[n - k] */
  uint32_t t;
  int mode;
  int32_t mu;
  int64_t energy;
  int64_t res;
} Model;

static void model_init(Model *m, uint32_t t, int32_t mu, int mode) {
  memset(m, 0, sizeof *m);
  m->t = t;
  m->mu = mu;
  m->mode = mode;
  for (uint32_t k = 0U; k < t; k++) {
    m->w[k] = 1 << 14;
  }
}

static void model_step(Model *m, int32_t x, int32_t d, int32_t *y, int32_t *e) {
  for (uint32_t k = m->t; k > 0U; k--) {
    m->hist[k] = m->hist[k - 1U];
  }
  m->hist[0] = x;
  int64_t acc = 0;
  for (uint32_t k = 0U; k < m->t; k++) {
    acc += (int64_t)(m->w[k] >> 15) * m->hist[k];
  }
  *y = sat(rdiv64(acc, 32768), -32768, 32767);
  *e = sat((int64_t)d - *y, -32768, 32767);
  m->energy += (int64_t)x * x - (int64_t)m->hist[m->t] * m->hist[m->t];
  int64_t step;
  if (m->mode == FD_LMS_NORM) {
    step = rdiv64((i128)m->mu * *e * 32768, (i128)m->energy + 1);
    step = step > 32767 ? 32767 : (step < -32767 ? -32767 : step);
  } else if (m->mode == FD_LMS_SIGN) {
    step = *e > 0 ? m->mu : (*e < 0 ? -m->mu : 0);
  } else {
    int64_t v = (int64_t)m->mu * *e + m->res; /* the last rounding carried */
    step = rdiv64(v, 32768);
    m->res = v - step * 32768;
  }
  for (uint32_t k = 0U; k < m->t; k++) {
    m->w[k] =
        sat((int64_t)m->w[k] + step * m->hist[k], -(1 << 30), (1 << 30) - 1);
  }
}

/* a target filter's output plus noise, from x */
static void target(const fm_q15 *x, fm_q15 *d, uint32_t n, int kind) {
  static const int32_t h[5] = {9000, -14000, 6000, 2500, -1200};
  for (uint32_t i = 0U; i < n; i++) {
    int64_t s = 0;
    for (uint32_t k = 0U; k < 5U && k <= i; k++) {
      s += (int64_t)h[k] * x[i - k];
    }
    int64_t v = (s >> 15) + (int32_t)(test_rng_u32(&rng) % 64U) - 32;
    if (kind == 2) {
      v = (int32_t)(test_rng_u32(&rng) >> 16) - 32768; /* no relation */
    }
    d[i] = (fm_q15)sat(v * (kind == 1 ? 8 : 1), -32768, 32767);
  }
}

void test_fd_lms_q15(void) {
  static fm_q15 x[NS], d[NS], y[NS], e[NS], st[MAXT + 80U], h[MAXT];
  static int32_t w[MAXT];
  static const uint32_t taps[] = {1U, 2U, 3U, 5U, 16U, 33U};
  static const int32_t mus[] = {0, 1, 300, 4096, 32767};
  for (unsigned ti = 0U; ti < 6U; ti++) {
    uint32_t t = taps[ti];
    for (int mode = 0; mode < 3; mode++) {
      for (unsigned mi = 0U; mi < 5U; mi++) {
        for (int kind = 0; kind < 3; kind++) {
          for (uint32_t i = 0U; i < NS; i++) {
            uint32_t u = test_rng_u32(&rng);
            x[i] = (fm_q15)(kind == 1 ? ((u & 1U) != 0U ? 32767 : -32768)
                                      : (int32_t)(u >> 16) - 32768);
            if (i % 97U < 9U) {
              x[i] = 0; /* quiet stretches: tiny energy for NORM */
            }
          }
          target(x, d, NS, kind);
          FD_LmsQ15 f;
          uint32_t chunk = 1U + test_rng_u32(&rng) % 79U;
          fd_lms_init_q15(&f, w, t, st, FD_LMS_STATE_LEN(t, chunk),
                          (fm_q15)mus[mi], (FD_LmsMode)mode);
          Model m;
          model_init(&m, t, mus[mi], mode);
          for (uint32_t i = 0U; i < NS;) {
            uint32_t p = 1U + test_rng_u32(&rng) % 50U;
            p = p < NS - i ? p : NS - i;
            fd_lms_q15(&f, x + i, d + i, (i & 1U) != 0U ? NULL : y + i, e + i,
                       p);
            for (uint32_t j = i; j < i + p; j++) {
              int32_t my, me;
              model_step(&m, x[j], d[j], &my, &me);
              if ((i & 1U) == 0U) {
                ASSERT_EQ(y[j], my);
              }
              ASSERT_EQ(e[j], me);
            }
            i += p;
            if (test_rng_u32(&rng) % 4U == 0U) {
              /* reading the taps applies the owed update: the same thing */
              fd_lms_coefs_q15(&f, h);
              for (uint32_t k = 0U; k < t; k++) {
                int32_t u = m.w[k];
                ASSERT_EQ(h[k], u >> 15);
              }
            }
          }
        }
      }
    }
  }
}

/* x'x + 1 = 2^17 and mu e = -+6: the normalised step is exactly -+1.5,
 * rounded to -1 and 2 (ties up) */
void test_fd_lms_ties(void) {
  static const fm_q15 x[4] = {1, 1, 5, 362};
  static fm_q15 st[8];
  static int32_t w[4];
  for (int sign = -1; sign <= 1; sign += 2) {
    fm_q15 d[4] = {0, 0, 0, (fm_q15)(3 * sign)};
    FD_LmsQ15 f;
    fd_lms_init_q15(&f, w, 4U, st, 8U, 2, FD_LMS_NORM);
    fd_lms_q15(&f, x, d, NULL, NULL, 4U);
    ASSERT_EQ(f.energy, 131071U);
    ASSERT_EQ(f.step, sign < 0 ? -1 : 2);
  }
}

/* identify a 5-tap system: the weights reach it */
void test_fd_lms_converge(void) {
  static fm_q15 x[8000], d[8000], st[64], h[16];
  static int32_t w[16];
  for (uint32_t i = 0U; i < 8000U; i++) {
    x[i] = (fm_q15)((int32_t)(test_rng_u32(&rng) >> 18) - 8192);
  }
  target(x, d, 8000U, 0);
  static const int32_t want[5] = {9000, -14000, 6000, 2500, -1200};
  for (int mode = 0; mode < 3; mode++) {
    FD_LmsQ15 f;
    fd_lms_init_q15(&f, w, 8U, st, 64U,
                    mode == 1 ? 2000 : (mode == 2 ? 64 : 8000),
                    (FD_LmsMode)mode);
    fd_lms_q15(&f, x, d, NULL, NULL, 8000U);
    fd_lms_coefs_q15(&f, h);
    double worst = 0.0;
    for (uint32_t k = 0U; k < 8U; k++) {
      double dk = fabs((double)h[k] - (k < 5U ? want[k] : 0));
      worst = dk > worst ? dk : worst;
    }
    printf("  lms q15 mode %d, 8 taps after 8000 samples: worst tap %.0f LSB\n",
           mode, worst);
    ASSERT_TRUE(worst < 200.0);
  }
}

/* ---- q31 ---- */

typedef struct {
  int32_t w[MAXT];
  int32_t hist[MAXT + 1U];
  uint32_t t;
  int mode;
  int32_t mu;
  i128 energy;
} Model31;

static int32_t rnd31(i128 v) { return (int32_t)rdiv64(v, (i128)1 << 31); }

static void model31_step(Model31 *m, int32_t x, int32_t d, int32_t *y,
                         int32_t *e) {
  for (uint32_t k = m->t; k > 0U; k--) {
    m->hist[k] = m->hist[k - 1U];
  }
  m->hist[0] = x;
  i128 acc = 0;
  for (uint32_t k = 0U; k < m->t; k++) {
    acc += (i128)m->w[k] * m->hist[k];
  }
  i128 r = acc + ((i128)1 << 29);
  r >>= 30;
  *y = r > INT32_MAX ? INT32_MAX : (r < INT32_MIN ? INT32_MIN : (int32_t)r);
  *e = sat((int64_t)d - *y, INT32_MIN, INT32_MAX);
  m->energy += (i128)x * x - (i128)m->hist[m->t] * m->hist[m->t];
  int64_t step;
  if (m->mode == FD_LMS_NORM) {
    i128 num = (i128)m->mu * *e * ((i128)1 << 31), den = m->energy + 1;
    i128 q;
    if ((num < 0 ? -num : num) >= den * (((i128)1 << 31) - 1)) {
      q = num < 0 ? -(i128)INT32_MAX : INT32_MAX;
    } else {
      i128 t = 2 * num + den;
      q = t / (2 * den);
      q = q * 2 * den > t ? q - 1 : q;
    }
    step = (int64_t)(q > INT32_MAX ? INT32_MAX
                                   : (q < -INT32_MAX ? -INT32_MAX : q));
  } else if (m->mode == FD_LMS_SIGN) {
    step = *e > 0 ? m->mu : (*e < 0 ? -m->mu : 0);
  } else {
    step = rnd31((i128)m->mu * *e);
  }
  for (uint32_t k = 0U; k < m->t; k++) {
    i128 v = (i128)m->w[k] + rdiv64((i128)step * m->hist[k], (i128)1 << 32);
    m->w[k] = sat((int64_t)v, -(1 << 30), (1 << 30) - 1);
  }
}

void test_fd_lms_q31(void) {
  static fm_q31 x[NS], d[NS], y[NS], e[NS], st[MAXT + 80U], h[MAXT];
  static int32_t w[MAXT];
  static const uint32_t taps[] = {1U, 2U, 3U, 5U, 16U, 33U};
  static const int32_t mus[] = {0, 1, 1 << 20, 1 << 27, INT32_MAX};
  for (unsigned ti = 0U; ti < 6U; ti++) {
    uint32_t t = taps[ti];
    for (int mode = 0; mode < 3; mode++) {
      for (unsigned mi = 0U; mi < 5U; mi++) {
        for (int kind = 0; kind < 3; kind++) {
          for (uint32_t i = 0U; i < NS; i++) {
            uint32_t u = test_rng_u32(&rng);
            x[i] = kind == 1 ? ((u & 1U) != 0U ? INT32_MAX : INT32_MIN)
                             : (fm_q31)u;
            if (i % 97U < 9U) {
              x[i] = 0;
            }
            uint32_t v = test_rng_u32(&rng);
            d[i] =
                kind == 2
                    ? (fm_q31)v
                    : (fm_q31)(((int64_t)x[i] + (i > 0U ? x[i - 1U] : 0)) / 3 +
                               (int32_t)(v % 4096U));
            if (kind == 1) {
              d[i] = (v & 1U) != 0U ? INT32_MIN : INT32_MAX;
            }
          }
          FD_LmsQ31 f;
          uint32_t chunk = 1U + test_rng_u32(&rng) % 79U;
          fd_lms_init_q31(&f, w, t, st, FD_LMS_STATE_LEN(t, chunk), mus[mi],
                          (FD_LmsMode)mode);
          Model31 m;
          memset(&m, 0, sizeof m);
          m.t = t;
          m.mu = mus[mi];
          m.mode = mode;
          for (uint32_t i = 0U; i < NS;) {
            uint32_t p = 1U + test_rng_u32(&rng) % 50U;
            p = p < NS - i ? p : NS - i;
            fd_lms_q31(&f, x + i, d + i, y + i, (i & 1U) != 0U ? NULL : e + i,
                       p);
            for (uint32_t j = i; j < i + p; j++) {
              int32_t my, me;
              model31_step(&m, x[j], d[j], &my, &me);
              ASSERT_EQ(y[j], my);
              if ((i & 1U) == 0U) {
                ASSERT_EQ(e[j], me);
              }
            }
            i += p;
            if (test_rng_u32(&rng) % 4U == 0U) {
              fd_lms_coefs_q31(&f, h);
              for (uint32_t k = 0U; k < t; k++) {
                ASSERT_EQ(h[k], m.w[k] * 2);
              }
            }
          }
        }
      }
    }
  }
}
