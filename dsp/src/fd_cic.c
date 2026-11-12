/**
 * @file fd_cic.c
 * @brief CIC decimator in wrapping 32- or 64-bit registers, and its
 * compensation filter design
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_fir_core.h"
#include "fd_resample.h"

FD_Status fd_cic_init(FD_Cic *c, uint32_t order, uint32_t ratio,
                      uint32_t in_bits) {
  FX_ASSERT(order >= 1U && order <= FD_CIC_MAX_ORDER && ratio >= 1U &&
            in_bits >= 1U && in_bits <= 32U);
  /* the least g with 2^g >= ratio^order */
  uint64_t p = 1U;
  uint32_t g = 0U;
  for (uint32_t i = 0U; i < order; i++) {
    if (p > UINT64_MAX / ratio) {
      return FD_ERR_RANGE;
    }
    p *= ratio;
    while (g < 64U && (p - 1U) >> g != 0U) {
      g++;
    }
    if (g + in_bits > 64U) {
      return FD_ERR_RANGE;
    }
  }
  uint32_t b = in_bits + g;
  c->order = order;
  c->ratio = ratio;
  c->growth = g;
  c->wide = b > 32U ? 1U : 0U;
  c->shift = b > 32U ? b - 32U : 32U - b;
  fd_cic_reset(c);
  return FD_OK;
}

void fd_cic_reset(FD_Cic *c) {
  for (uint32_t i = 0U; i < FD_CIC_MAX_ORDER; i++) {
    c->integ[i] = 0U;
    c->comb[i] = 0U;
  }
  c->count = 0U;
}

/* n inputs through the integrators, order a constant so they stay in
 * registers; the input is x15 when q15, else x31 */
FX_INLINE void run32(uint64_t *g, const fm_q15 *x15, const fm_q31 *x31,
                     bool q15, uint32_t n, uint32_t order) {
  uint32_t a[FD_CIC_MAX_ORDER];
  FX_UNROLL(6)
  for (uint32_t s = 0U; s < order; s++) {
    a[s] = (uint32_t)g[s];
  }
  FX_UNROLL(8)
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t v = q15 ? (uint32_t)(int32_t)x15[i] : (uint32_t)x31[i];
    FX_UNROLL(6)
    for (uint32_t s = 0U; s < order; s++) {
      a[s] += v;
      v = a[s];
    }
  }
  FX_UNROLL(6)
  for (uint32_t s = 0U; s < order; s++) {
    g[s] = a[s];
  }
}

FX_INLINE void run64(uint64_t *g, const fm_q15 *x15, const fm_q31 *x31,
                     bool q15, uint32_t n, uint32_t order) {
  uint64_t a[FD_CIC_MAX_ORDER];
  FX_UNROLL(6)
  for (uint32_t s = 0U; s < order; s++) {
    a[s] = g[s];
  }
  FX_UNROLL(8)
  for (uint32_t i = 0U; i < n; i++) {
    uint64_t v = (uint64_t)(int64_t)(q15 ? x15[i] : x31[i]);
    FX_UNROLL(6)
    for (uint32_t s = 0U; s < order; s++) {
      a[s] += v;
      v = a[s];
    }
  }
  FX_UNROLL(6)
  for (uint32_t s = 0U; s < order; s++) {
    g[s] = a[s];
  }
}

#define RUN(fn, o) fn(c->integ, x15, x31, q15, n, o)

FX_INLINE void integrate(FD_Cic *c, const fm_q15 *x15, const fm_q31 *x31,
                         bool q15, uint32_t n) {
  if (c->wide != 0U) {
    switch (c->order) {
    case 1U:
      RUN(run64, 1U);
      break;
    case 2U:
      RUN(run64, 2U);
      break;
    case 3U:
      RUN(run64, 3U);
      break;
    case 4U:
      RUN(run64, 4U);
      break;
    case 5U:
      RUN(run64, 5U);
      break;
    default:
      RUN(run64, 6U);
      break;
    }
  } else {
    switch (c->order) {
    case 1U:
      RUN(run32, 1U);
      break;
    case 2U:
      RUN(run32, 2U);
      break;
    case 3U:
      RUN(run32, 3U);
      break;
    case 4U:
      RUN(run32, 4U);
      break;
    case 5U:
      RUN(run32, 5U);
      break;
    default:
      RUN(run32, 6U);
      break;
    }
  }
}

/* the combs on the last integrator: the exact result, aligned to q31 */
static fm_q31 comb(FD_Cic *c) {
  uint64_t v = c->integ[c->order - 1U];
  if (c->wide == 0U) {
    uint32_t w = (uint32_t)v;
    for (uint32_t s = 0U; s < c->order; s++) {
      uint32_t d = w - (uint32_t)c->comb[s];
      c->comb[s] = w;
      w = d;
    }
    return (fm_q31)(w << c->shift);
  }
  for (uint32_t s = 0U; s < c->order; s++) {
    uint64_t d = v - c->comb[s];
    c->comb[s] = v;
    v = d;
  }
  /* under 2^31 - 1/2 before rounding, even at the largest input */
  return (fm_q31)((((int64_t)v >> (c->shift - 1U)) + 1) >> 1);
}

FX_INLINE uint32_t decim(FD_Cic *c, const fm_q15 *x15, const fm_q31 *x31,
                         bool q15, fm_q31 *y, uint32_t n) {
  uint32_t out = 0U;
  while (n != 0U) {
    uint32_t run = c->ratio - c->count;
    run = run < n ? run : n;
    integrate(c, x15, x31, q15, run);
    if (q15) {
      x15 += run;
    } else {
      x31 += run;
    }
    n -= run;
    c->count += run;
    if (c->count == c->ratio) {
      c->count = 0U;
      y[out++] = comb(c);
    }
  }
  return out;
}

uint32_t fd_cic_decim_q15(FD_Cic *c, const fm_q15 *x, fm_q31 *y, uint32_t n) {
  return decim(c, x, NULL, true, y, n);
}

uint32_t fd_cic_decim_q31(FD_Cic *c, const fm_q31 *x, fm_q31 *y, uint32_t n) {
  return decim(c, NULL, x, false, y, n);
}

int32_t fd_cic_gain(const FD_Cic *c) {
  /* ratio^order <= 2^growth <= 2^63: exact in 64 bits */
  uint64_t p = 1U;
  for (uint32_t i = 0U; i < c->order; i++) {
    p *= c->ratio;
  }
  const uint32_t g = c->growth;
  if (g <= 30U) {
    return (int32_t)(p << (30U - g));
  }
  return (int32_t)((p + (1ULL << (g - 31U))) >> (g - 30U));
}

FD_Status fd_cic_comp_design(float *h, uint32_t ntaps, uint32_t order,
                             uint32_t ratio, float fc, FD_Window win) {
  FX_ASSERT(ntaps >= 1U && order >= 1U && order <= FD_CIC_MAX_ORDER &&
            ratio >= 1U && (uint32_t)win <= (uint32_t)FD_WIN_BARTLETT);
  if (!(fc > 0.0f && fc < 0.5f)) {
    return FD_ERR_RANGE;
  }
  /* h[k] = 2 integral over [0, fc] of D(f) cos(2 pi f m), D = 1 / |CIC| */
  const uint32_t steps = 256U;
  const float df = fc / (float)steps, c0 = 0.5f * (float)(ntaps - 1U);
  float sum = 0.0f;
  for (uint32_t k = 0U; k < ntaps; k++) {
    float m = (float)k - c0, acc = 0.0f;
    for (uint32_t i = 0U; i <= steps; i++) {
      float f = df * (float)i, d = 1.0f;
      if (i != 0U && ratio > 1U) {
        float r =
            (float)ratio * fm_sinf(PI_F * f / (float)ratio) / fm_sinf(PI_F * f);
        d = 1.0f;
        for (uint32_t o = 0U; o < order; o++) {
          d *= r;
        }
      }
      float wgt = i == 0U || i == steps ? 0.5f : 1.0f; /* trapezoid */
      acc += wgt * d * fm_cosf(2.0f * PI_F * f * m);
    }
    h[k] = 2.0f * acc * df * win_value(win, k, ntaps);
    sum += h[k];
  }
  if (!(sum > 1e-6f || sum < -1e-6f)) {
    return FD_ERR_RANGE;
  }
  const float s = 1.0f / sum;
  for (uint32_t k = 0U; k < ntaps; k++) {
    h[k] *= s;
  }
  return FD_OK;
}
