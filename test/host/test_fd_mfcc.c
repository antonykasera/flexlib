/**
 * @file test_fd_mfcc.c
 * @brief Host tests for fd_mfcc.h against double designs and exact sums
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_mfcc.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>

__extension__ typedef __int128 i128;

#define NMAX 1024U
#define BMAX (NMAX / 2U)

static uint16_t cnt[BMAX + 1U], w[NMAX / 2U + 2U];
static fm_q15 spec[NMAX + 2U] FX_ALIGN(4);
static uint64_t y[BMAX + 1U];
static fm_q31 tab[64U * 64U];
static uint64_t rng = 0x3E1F00DU;

static uint32_t pick(uint32_t lo, uint32_t hi) {
  return lo + test_rng_u32(&rng) % (hi - lo + 1U);
}

static double design_error(uint32_t n, uint32_t rate, uint32_t bands,
                           uint32_t f_lo, uint32_t f_hi, unsigned *moved) {
  double lo = log2(700.0 * n + (double)f_lo * n);
  double hi = log2(700.0 * n + (double)f_hi * n);
  FD_Mel m;
  ASSERT_EQ(fd_mel_init(&m, cnt, w, n, rate, bands, f_lo, f_hi), FD_OK);
  double worst = 0.0;
  uint32_t i = 0U, k = 0U;
  while (k <= n / 2U && log2(700.0 * n + (double)k * rate) < lo - 1e-9) {
    k++;
  }
  for (uint32_t j = 0U; j <= bands; j++) {
    double a = lo + (hi - lo) * j / (bands + 1U);
    double b = lo + (hi - lo) * (j + 1U) / (bands + 1U);
    for (uint32_t c = 0U; c < cnt[j]; c++, i++) {
      double v = log2(700.0 * n + (double)(m.k0 + i) * rate);
      double want = (v - a) / (b - a) * 32768.0;
      if (v < a - 1e-9 || v >= b + 1e-9) {
        (*moved)++;
        continue;
      }
      double d = fabs(w[i] - want);
      worst = d > worst ? d : worst;
    }
  }
  (void)k;
  return worst;
}

void test_fd_mel_design(void) {
  static const uint32_t rates[] = {8000U, 16000U, 22050U, 44100U, 48000U, 64000U};
  double worst = 0.0;
  unsigned moved = 0U;
  for (unsigned t = 0U; t < 400U; t++) {
    uint32_t n = 4U << pick(0U, 8U), rate = rates[pick(0U, 5U)];
    uint32_t f_hi = pick(2U, rate / 2U), f_lo = pick(0U, f_hi - 1U);
    uint32_t bands = pick(1U, n / 2U < 80U ? n / 2U : 80U);
    double d = design_error(n, rate, bands, f_lo, f_hi, &moved);
    worst = d > worst ? d : worst;
  }
  ASSERT_TRUE(worst <= 0.501);
  ASSERT_EQ(moved, 0U);
}

void test_fd_mel_init_range(void) {
  FD_Mel m;
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 512U, 16000U, 40U, 20U, 4000U), FD_OK);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 512U, 16000U, 40U, 20U, 8000U), FD_OK);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 512U, 16000U, 40U, 20U, 8001U), FD_ERR_RANGE);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 512U, 16000U, 40U, 4000U, 4000U), FD_ERR_RANGE);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 512U, 16000U, 0U, 20U, 4000U), FD_ERR_RANGE);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 512U, 16000U, 257U, 20U, 4000U), FD_ERR_RANGE);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 384U, 16000U, 40U, 20U, 4000U), FD_ERR_RANGE);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 2U, 16000U, 1U, 20U, 4000U), FD_ERR_RANGE);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 512U, 0U, 40U, 0U, 1U), FD_ERR_RANGE);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 512U, 64001U, 40U, 20U, 4000U), FD_ERR_RANGE);
  ASSERT_EQ(fd_mel_init(&m, cnt, w, 4U, 8000U, 2U, 0U, 4000U), FD_OK);
  uint32_t total = 0U;
  for (uint32_t j = 0U; j <= 2U; j++) {
    total += cnt[j];
  }
  ASSERT_EQ(m.k0 + total, 2U);
}

static unsigned mel_case(uint32_t n, uint32_t bands, int extreme) {
  FD_Mel m;
  uint32_t rate = 16000U, f_hi = pick(100U, 8000U), f_lo = pick(0U, f_hi - 1U);
  if (fd_mel_init(&m, cnt, w, n, rate, bands, f_lo, f_hi) != FD_OK) {
    return 1U;
  }
  for (uint32_t i = 0U; i < n; i++) {
    spec[i] = extreme ? (test_rng_u32(&rng) & 1U ? -32768 : 32767)
                      : (fm_q15)test_rng_u32(&rng);
  }
  int32_t e = (int32_t)pick(0U, 20U) - 10;
  int32_t r = fd_mel_q15(&m, spec, e, y);
  unsigned bad = r != 2 * e - 15;
  uint32_t k = m.k0, i = 0U;
  uint64_t want[BMAX + 1U] = {0};
  for (uint32_t j = 0U; j <= bands; j++) {
    for (uint32_t c = 0U; c < cnt[j]; c++, k++, i++) {
      int64_t re = spec[2U * k], im = k == 0U ? 0 : spec[2U * k + 1U];
      uint64_t p = (uint64_t)(re * re + im * im);
      if (j < bands) {
        want[j] += w[i] * p;
      }
      if (j > 0U) {
        want[j - 1U] += (32768U - w[i]) * p;
      }
    }
  }
  for (uint32_t j = 0U; j < bands; j++) {
    bad += y[j] != want[j];
  }
  return bad;
}

void test_fd_mel_sums(void) {
  unsigned bad = 0U;
  for (unsigned t = 0U; t < 600U; t++) {
    uint32_t n = 8U << pick(0U, 7U);
    bad += mel_case(n, pick(1U, n / 2U < 64U ? n / 2U : 64U), t % 5U == 0U);
  }
  ASSERT_EQ(bad, 0U);
}

static double log_error(uint64_t v, int32_t e, int32_t *got) {
  fd_log_q16(&v, e, got, 1U);
  long double want =
      (logl((long double)(v != 0U ? v : 1U)) + e * logl(2.0L)) * 65536.0L;
  if (want >= 2147483647.0L) {
    return *got == INT32_MAX ? 0.0 : 1e9;
  }
  if (want <= -2147483648.0L) {
    return *got == INT32_MIN ? 0.0 : 1e9;
  }
  return (double)fabsl((long double)*got - want);
}

void test_fd_log(void) {
  double worst = 0.0;
  int32_t got;
  for (unsigned t = 0U; t < 400000U; t++) {
    uint64_t v = test_rng_next(&rng) >> pick(0U, 63U);
    int32_t e = (int32_t)pick(0U, 200U) - 100;
    double d = log_error(v, e, &got);
    worst = d > worst ? d : worst;
  }
  for (uint32_t s = 0U; s < 64U; s++) {
    for (int32_t e = -40; e <= 40; e += 7) {
      double d = log_error(1ULL << s, e, &got);
      worst = d > worst ? d : worst;
      d = log_error((1ULL << s) - 1U, e, &got);
      worst = d > worst ? d : worst;
    }
  }
  ASSERT_TRUE(worst <= 0.51);
  ASSERT_TRUE(log_error(0U, 0, &got) == 0.0 && got == 0);
  ASSERT_TRUE(log_error(1U, 47300, &got) == 0.0 && got == INT32_MAX);
  ASSERT_TRUE(log_error(1U, -47300, &got) == 0.0 && got == INT32_MIN);
  ASSERT_TRUE(log_error(UINT64_MAX, 47210, &got) <= 1.0);
  ASSERT_TRUE(log_error(1U, 47274, &got) <= 1.0 && got != INT32_MAX);
  ASSERT_TRUE(log_error(UINT64_MAX, INT32_MAX, &got) == 0.0);
  ASSERT_TRUE(log_error(1U, INT32_MIN, &got) == 0.0);
}

static fm_q31 ref_dct(const fm_q31 *x, const fm_q31 *c, uint32_t n) {
  i128 s = 0;
  for (uint32_t i = 0U; i < n; i++) {
    s += (i128)x[i] * c[i];
  }
  s = (s + ((i128)1 << 30)) >> 31;
  return s > INT32_MAX ? INT32_MAX : (s < INT32_MIN ? INT32_MIN : (fm_q31)s);
}

void test_fd_dct2(void) {
  static fm_q31 x[64], out[64];
  FD_Dct d;
  double worst = 0.0;
  unsigned bad = 0U, wide = 0U;
  for (uint32_t n = 2U; n <= 64U; n++) {
    uint32_t k = pick(1U, n);
    ASSERT_EQ(fd_dct2_init(&d, tab, n, k), FD_OK);
    for (uint32_t j = 0U; j < k; j++) {
      double s = sqrt((j == 0U ? 1.0 : 2.0) / n);
      for (uint32_t i = 0U; i < n; i++) {
        double want = s * cos(3.14159265358979323846 * j * (2.0 * i + 1.0) / (2.0 * n)) * 2147483648.0;
        double e = fabs(tab[j * n + i] - want);
        worst = e > worst ? e : worst;
      }
    }
    for (unsigned t = 0U; t < 60U; t++) {
      uint32_t sh = pick(0U, 31U);
      uint64_t sum = 0U;
      for (uint32_t i = 0U; i < n; i++) {
        x[i] = t % 7U == 0U ? (test_rng_u32(&rng) & 1U ? INT32_MIN : INT32_MAX)
                            : (fm_q31)test_rng_u32(&rng) >> sh;
        sum += (uint64_t)(x[i] < 0 ? -(int64_t)x[i] : x[i]);
      }
      wide += sum >= (1ULL << 32);
      fd_dct2_q31(&d, x, out);
      for (uint32_t j = 0U; j < k; j++) {
        bad += out[j] != ref_dct(x, tab + j * n, n);
      }
    }
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(wide > 500U);
  ASSERT_TRUE(worst <= 1.5);
  ASSERT_EQ(fd_dct2_init(&d, tab, 1U, 1U), FD_ERR_RANGE);
  ASSERT_EQ(fd_dct2_init(&d, tab, 8U, 0U), FD_ERR_RANGE);
  ASSERT_EQ(fd_dct2_init(&d, tab, 8U, 9U), FD_ERR_RANGE);
}
