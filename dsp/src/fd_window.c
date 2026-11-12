/**
 * @file fd_window.c
 * @brief Window functions, generated at run time for any length
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_window.h"
#include "fd_vec.h"
#include "fd_wide.h"
#include "fm_float.h"
#include "fm_trig.h"
#include <stddef.h>

/* w = a0 - a1 cos t + a2 cos 2t - a3 cos 3t + a4 cos 4t, a in q31 */
static const int32_t cos_sum[][5] = {
    {0, 0, 0, 0, 0}, /* rectangular: not a cosine sum */
    {1073741824, 1073741824, 0, 0, 0},
    {1159641170, 987842478, 0, 0, 0},
    {901943132, 1073741824, 171798692, 0, 0},
    {770409759, 1048594790, 303396490, 25082609, 0},
    {764005962, 1046674940, 309735862, 27066884, 0},
    {462952270, 894709505, 595418098, 179484422, 14919359},
};

FX_INLINE void put(fm_q31 *w31, fm_q15 *w15, uint32_t k, int32_t v) {
  if (w31 != NULL) {
    w31[k] = v;
  } else { /* round q31 to q15 */
    w15[k] = fm_sat_q15((int32_t)(((int64_t)v + 0x8000) >> 16));
  }
}

/* writes w[k] and its mirror: D - k (symmetric) or n - k (periodic) */
FX_INLINE void put2(fm_q31 *w31, fm_q15 *w15, uint32_t n, uint32_t m,
                    uint32_t k, int32_t v) {
  put(w31, w15, k, v);
  if (m != k && m < n) {
    put(w31, w15, m, v);
  }
}

/* angles round(j k 2^32 / d) for j = 1..4, stepped exactly in k */
typedef struct {
  uint32_t a[4], r[4], qs[4], rs[4];
} Angles;

static void angles_init(Angles *g, uint32_t d) {
  for (uint32_t j = 0U; j < 4U; j++) {
    uint64_t t = (uint64_t)(j + 1U) << 32;
    g->qs[j] = (uint32_t)(t / d);
    g->rs[j] = (uint32_t)(t % d);
    g->a[j] = 0U;
    g->r[j] = 0U;
  }
}

FX_INLINE uint32_t angle(const Angles *g, uint32_t j, uint32_t d) {
  return g->a[j] + (2ULL * g->r[j] >= d ? 1U : 0U);
}

FX_INLINE void angles_step(Angles *g, uint32_t terms, uint32_t d) {
  for (uint32_t j = 0U; j < terms; j++) {
    g->a[j] += g->qs[j];
    g->r[j] += g->rs[j];
    if (g->r[j] >= d) {
      g->r[j] -= d;
      g->a[j]++;
    }
  }
}

FX_INLINE void generate(fm_q31 *w31, fm_q15 *w15, uint32_t n, FD_Window type,
                        bool periodic) {
  FX_ASSERT(n >= 1U && (uint32_t)type <= (uint32_t)FD_WIN_BARTLETT);
  const uint32_t d = periodic ? n : n - 1U;
  if (type == FD_WIN_RECT || d == 0U) {
    for (uint32_t k = 0U; k < n; k++) {
      put(w31, w15, k, INT32_MAX);
    }
    return;
  }
  Angles g;
  angles_init(&g, d);
  const uint32_t half = d / 2U;
  if (type == FD_WIN_BARTLETT) { /* 2k / d up to the middle */
    for (uint32_t k = 0U; k <= half; k++) {
      uint32_t v = angle(&g, 0U, d);
      put2(w31, w15, n, d - k, k,
           v > (uint32_t)INT32_MAX ? INT32_MAX : (int32_t)v);
      angles_step(&g, 1U, d);
    }
    return;
  }
  const int32_t *a = cos_sum[type];
  const uint32_t terms =
      a[4] != 0 ? 4U : (a[3] != 0 ? 3U : (a[2] != 0 ? 2U : 1U));
  for (uint32_t k = 0U; k <= half; k++) {
    int64_t s = (int64_t)a[0] << 31;
    for (uint32_t j = 0U; j < terms; j++) {
      uint32_t t = angle(&g, j, d);
      int64_t c =
          t == 0U ? 0x80000000LL : fm_cos_q31(t); /* cos 0 = 1 exactly */
      int64_t p = a[j + 1U] * c;
      s += (j & 1U) != 0U ? p : -p;
    }
    int64_t v = (s + 0x40000000) >> 31;
    put2(w31, w15, n, d - k, k, v > INT32_MAX ? INT32_MAX : (int32_t)v);
    angles_step(&g, terms, d);
  }
}

void fd_window_q15(fm_q15 *w, uint32_t n, FD_Window type, bool periodic) {
  generate(NULL, w, n, type, periodic);
}

void fd_window_q31(fm_q31 *w, uint32_t n, FD_Window type, bool periodic) {
  generate(w, NULL, n, type, periodic);
}

/* ---- parametric windows, in float ---- */

typedef enum { KAISER, GAUSS, TUKEY } Param;

/* I0(x) by its series, to float precision */
static float bessel_i0(float x) {
  float t = 1.0f, s = 1.0f, h = 0.25f * x * x;
  for (uint32_t m = 1U; m < 200U && t > 1e-9f * s; m++) {
    t *= h / ((float)m * (float)m);
    s += t;
  }
  return s;
}

static void param(fm_q31 *w31, fm_q15 *w15, uint32_t n, Param kind, float p,
                  bool periodic) {
  FX_ASSERT(n >= 1U);
  const uint32_t d = periodic ? n : n - 1U;
  const float i0b = kind == KAISER ? bessel_i0(p) : 1.0f;
  for (uint32_t k = 0U; k <= d / 2U && k < n; k++) {
    float v = 1.0f;
    if (d != 0U) {
      float r = (2.0f * (float)k - (float)d) / (float)d; /* -1 .. 0 */
      if (kind == KAISER) {
        v = bessel_i0(p * fm_sqrtf(1.0f - r * r)) / i0b;
      } else if (kind == GAUSS) {
        float z = r / p;
        v = fm_expf(-0.5f * z * z);
      } else if ((float)k < 0.5f * p * (float)d) { /* the Tukey taper */
        v = 0.5f * (1.0f - fm_cosf(6.28318531f * (float)k / (p * (float)d)));
      }
    }
    int32_t q = fm_f32_to_q31(v);
    put2(w31, w15, n, d - k, k, q);
  }
}

void fd_window_kaiser_q15(fm_q15 *w, uint32_t n, float beta, bool periodic) {
  FX_ASSERT(beta >= 0.0f);
  param(NULL, w, n, KAISER, beta, periodic);
}

void fd_window_kaiser_q31(fm_q31 *w, uint32_t n, float beta, bool periodic) {
  FX_ASSERT(beta >= 0.0f);
  param(w, NULL, n, KAISER, beta, periodic);
}

void fd_window_gauss_q15(fm_q15 *w, uint32_t n, float sigma, bool periodic) {
  FX_ASSERT(sigma > 0.0f);
  param(NULL, w, n, GAUSS, sigma, periodic);
}

void fd_window_gauss_q31(fm_q31 *w, uint32_t n, float sigma, bool periodic) {
  FX_ASSERT(sigma > 0.0f);
  param(w, NULL, n, GAUSS, sigma, periodic);
}

void fd_window_tukey_q15(fm_q15 *w, uint32_t n, float alpha, bool periodic) {
  FX_ASSERT(alpha >= 0.0f && alpha <= 1.0f);
  param(NULL, w, n, TUKEY, alpha, periodic);
}

void fd_window_tukey_q31(fm_q31 *w, uint32_t n, float alpha, bool periodic) {
  FX_ASSERT(alpha >= 0.0f && alpha <= 1.0f);
  param(w, NULL, n, TUKEY, alpha, periodic);
}

/* ---- gains ---- */

/* round(num / den), ties up, num >= 0 or not */
static fm_q16 ratio(int64_t num, uint64_t den) {
  int64_t q = num / (int64_t)den, r = num % (int64_t)den;
  if (r < 0) {
    r += (int64_t)den;
    q--;
  }
  q += 2 * (uint64_t)r >= den ? 1 : 0;
  return q > INT32_MAX ? INT32_MAX : (fm_q16)q;
}

void fd_window_gains_q15(const fm_q15 *w, uint32_t n, fm_q16 *coherent,
                         fm_q16 *power) {
  FX_ASSERT(n >= 1U);
  *coherent = ratio(2 * fd_sum_q15(w, n), n);
  *power = ratio((int64_t)fd_power_q15(w, n), (uint64_t)n << 14);
}

void fd_window_gains_q31(const fm_q31 *w, uint32_t n, fm_q16 *coherent,
                         fm_q16 *power) {
  FX_ASSERT(n >= 1U);
  *coherent = ratio(fd_sum_q31(w, n), (uint64_t)n << 15);
  uint64_t p48 = fd_power_q31(w, n); /* Q48, at most n 2^48 */
  FD_U128 t = fd_u128_add(fd_u128(0U, p48), fd_u128(0U, (uint64_t)n << 31));
  (void)fd_u128_divmod32(&t, n);
  t = fd_u128_shr(t, 32U);
  *power = t.lo > (uint64_t)INT32_MAX ? INT32_MAX : (fm_q16)t.lo;
}
