/**
 * @file accuracy.c
 * @brief `make accuracy`: flexmath and the CMSIS-DSP functions it replaces,
 *        both against a double reference; prints docs/accuracy.md
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "arm_math.h"
#include "fm_atan.h"
#include "fm_div.h"
#include "fm_exp.h"
#include "fm_fixed.h"
#include "fm_float.h"
#include "fm_sqrt.h"
#include "fm_trig.h"
#include "test_stats.h"
#include <stdlib.h>
#include <string.h>

static void row(const ErrStats *s, const char *impl, const char *inputs) {
  printf("| %s | %s | %s | %.3g | %.3g | %+.2g |\n", s->name, impl, inputs,
         s->max_abs, stats_mean_abs(s), stats_bias(s));
}

static void mul_q15(void) {
  ErrStats f, c;
  stats_init(&f, "multiply q15", "lsb");
  stats_init(&c, "multiply q15", "lsb");
  static q15_t a[65536], b[65536], out[65536];
  for (int32_t i = 0; i < 65536; i++) {
    b[i] = (q15_t)(i - 32768);
  }
  for (int32_t x = -32768; x < 32768; x++) {
    for (int32_t i = 0; i < 65536; i++) {
      a[i] = (q15_t)x;
    }
    arm_mult_q15(a, b, out, 65536U);
    for (int32_t i = 0; i < 65536; i++) {
      double exact = (double)x * b[i] / 32768.0;
      if (exact >= 32767.0) {
        continue; /* -1 * -1 saturates in both */
      }
      stats_add(&f, x, fm_mul_q15((fm_q15)x, b[i]) - exact);
      stats_add(&c, x, out[i] - exact);
    }
  }
  row(&f, "flexmath", "all 2^32 pairs");
  row(&c, "CMSIS-DSP", "all 2^32 pairs");
}

static void mul_q31(void) {
  ErrStats f, c;
  stats_init(&f, "multiply q31", "lsb");
  stats_init(&c, "multiply q31", "lsb");
  uint64_t st = 1U;
  for (int i = 0; i < 10000000; i++) {
    q31_t a = (q31_t)test_rng_u32(&st);
    q31_t b = (q31_t)test_rng_u32(&st);
    q31_t out;
    arm_mult_q31(&a, &b, &out, 1U);
    long double exact = (long double)a * b / 2147483648.0L;
    if (exact >= 2147483647.0L) {
      continue;
    }
    stats_add(&f, a, (double)((long double)fm_mul_q31(a, b) - exact));
    stats_add(&c, a, (double)((long double)out - exact));
  }
  row(&f, "flexmath", "10^7 random");
  row(&c, "CMSIS-DSP", "10^7 random");
}

/* float to q15 for every float in [-1, 1): error against x * 2^15 */
static void f32_to_q15(void) {
  ErrStats f, c;
  stats_init(&f, "float to q15", "lsb");
  stats_init(&c, "float to q15", "lsb");
  for (uint64_t u = 0U; u <= 0xFFFFFFFFU; u += 1U + (u & 0xFFU)) {
    float x;
    uint32_t b = (uint32_t)u;
    memcpy(&x, &b, sizeof x);
    if (!(x >= -1.0f && x < 1.0f - 1.0f / 65536.0f)) {
      continue;
    }
    q15_t out;
    arm_float_to_q15(&x, &out, 1U);
    double exact = (double)x * 32768.0;
    stats_add(&f, x, fm_f32_to_q15(x) - exact);
    stats_add(&c, x, out - exact);
  }
  row(&f, "flexmath", "floats in [-1, 1), every ~128th");
  row(&c, "CMSIS-DSP", "floats in [-1, 1), every ~128th");
}

/* q31 to float: error in ULPs of the float result */
static void q31_to_f32(void) {
  ErrStats f, c;
  stats_init(&f, "q31 to float", "ulp");
  stats_init(&c, "q31 to float", "ulp");
  for (uint64_t u = 0U; u <= 0xFFFFFFFFU; u += 1U + (u & 0xFFU)) {
    q31_t q = (q31_t)(uint32_t)u;
    float out;
    arm_q31_to_float(&q, &out, 1U);
    double exact = ldexp((double)q, -31);
    stats_add(&f, q, ulp_err_f32(fm_q31_to_f32(q), exact));
    stats_add(&c, q, ulp_err_f32(out, exact));
  }
  row(&f, "flexmath", "every ~128th q31");
  row(&c, "CMSIS-DSP", "every ~128th q31");
}

static void sqrt_q15(void) {
  ErrStats f, c;
  stats_init(&f, "sqrt q15", "lsb");
  stats_init(&c, "sqrt q15", "lsb");
  for (int32_t x = 1; x < 32768; x++) {
    q15_t out;
    arm_sqrt_q15((q15_t)x, &out);
    double exact = sqrt((double)x * 32768.0);
    stats_add(&f, x, fm_sqrt_q15((fm_q15)x) - exact);
    stats_add(&c, x, out - exact);
  }
  row(&f, "flexmath", "all 32767 positive");
  row(&c, "CMSIS-DSP", "all 32767 positive");
}

static void sqrt_q31(void) {
  ErrStats f, c;
  stats_init(&f, "sqrt q31", "lsb");
  stats_init(&c, "sqrt q31", "lsb");
  uint64_t st = 2U;
  for (int i = 0; i < 10000000; i++) {
    uint32_t u = test_rng_u32(&st);
    q31_t x = (q31_t)(u >> 1) >> (u & 31U);
    if (x <= 0) {
      continue;
    }
    q31_t out;
    arm_sqrt_q31(x, &out);
    long double exact = sqrtl((long double)x * 2147483648.0L);
    stats_add(&f, x, (double)((long double)fm_sqrt_q31(x) - exact));
    stats_add(&c, x, (double)((long double)out - exact));
  }
  row(&f, "flexmath", "10^7 random, every magnitude");
  row(&c, "CMSIS-DSP", "10^7 random, every magnitude");
}

/* CMSIS returns a quotient and a shift; with |a| < |b| the shift is 0 */
static void div_q15(void) {
  ErrStats f, c;
  stats_init(&f, "divide q15", "lsb");
  stats_init(&c, "divide q15", "lsb");
  for (int32_t b = -32768; b < 32768; b++) {
    for (int32_t a = -32768; a < 32768; a++) {
      if (b == 0 || abs(a) >= abs(b)) {
        continue;
      }
      q15_t out;
      int16_t sh;
      arm_divide_q15((q15_t)a, (q15_t)b, &out, &sh);
      double exact = (double)a * 32768.0 / b;
      stats_add(&f, a, fm_div_q15((fm_q15)a, (fm_q15)b) - exact);
      stats_add(&c, a, ldexp(out, sh) - exact);
    }
  }
  row(&f, "flexmath", "all pairs with abs(a) < abs(b)");
  row(&c, "CMSIS-DSP", "all pairs with abs(a) < abs(b)");
}

static void div_q31(void) {
  ErrStats f, c;
  stats_init(&f, "divide q31", "lsb");
  stats_init(&c, "divide q31", "lsb");
  uint64_t st = 3U;
  for (int i = 0; i < 10000000; i++) {
    uint32_t u = test_rng_u32(&st);
    q31_t a = (q31_t)u >> (u & 31U);
    q31_t b = (q31_t)test_rng_u32(&st);
    if (llabs((long long)a) >= llabs((long long)b)) {
      continue;
    }
    q31_t out;
    int16_t sh;
    arm_divide_q31(a, b, &out, &sh);
    long double exact = (long double)a * 2147483648.0L / b;
    stats_add(&f, a, (double)((long double)fm_div_q31(a, b) - exact));
    stats_add(&c, a, (double)(ldexpl(out, sh) - exact));
  }
  row(&f, "flexmath", "10^7 random with abs(a) < abs(b)");
  row(&c, "CMSIS-DSP", "10^7 random with abs(a) < abs(b)");
}

/* CMSIS angles are ours shifted right by one, so only even angles compare */
static void sin_q15(void) {
  ErrStats f, c;
  stats_init(&f, "sin q15", "lsb");
  stats_init(&c, "sin q15", "lsb");
  for (int32_t x = 0; x < 32768; x++) {
    uint32_t a = (uint32_t)x << 1;
    double exact = sin(6.283185307179586 * a / 65536.0) * 32768.0;
    exact = exact > 32767.0 ? 32767.0 : exact;
    stats_add(&f, a, fm_sin_q15((uint16_t)a) - exact);
    stats_add(&c, a, arm_sin_q15((q15_t)x) - exact);
  }
  row(&f, "flexmath", "all 32768 even angles");
  row(&c, "CMSIS-DSP", "all 32768 even angles");
}

static void sin_q31(void) {
  ErrStats f, c;
  stats_init(&f, "sin q31", "lsb");
  stats_init(&c, "sin q31", "lsb");
  uint64_t st = 4U;
  for (int i = 0; i < 10000000; i++) {
    uint32_t a = test_rng_u32(&st) & ~1U;
    long double exact =
        sinl(6.283185307179586476925L * a / 4294967296.0L) * 2147483648.0L;
    exact = exact > 2147483647.0L ? 2147483647.0L : exact;
    stats_add(&f, a, (double)((long double)fm_sin_q31(a) - exact));
    stats_add(&c, a,
              (double)((long double)arm_sin_q31((q31_t)(a >> 1)) - exact));
  }
  row(&f, "flexmath", "10^7 random even angles");
  row(&c, "CMSIS-DSP", "10^7 random even angles");
}

/* angle error in 2^-bits turns, mod a turn; CMSIS returns radians */
static double ang_err(double got_turns, double ref_rad, double turn) {
  double e = got_turns - ref_rad * (turn / 6.283185307179586);
  return e - turn * nearbyint(e / turn);
}

static void atan2_q15(void) {
  ErrStats f, c;
  stats_init(&f, "atan2 q15", "2^-16 turn");
  stats_init(&c, "atan2 q15", "2^-16 turn");
  for (int32_t y = -32768; y < 32768; y += 3) {
    for (int32_t x = -32768; x < 32768; x += 5) {
      q15_t out;
      arm_atan2_q15((q15_t)y, (q15_t)x, &out);
      double ref = atan2(y, x);
      stats_add(&f, y, ang_err(fm_atan2_q15((fm_q15)y, (fm_q15)x), ref, 65536));
      stats_add(&c, y,
                ang_err(out / 8192.0 * (65536 / 6.283185307179586), ref,
                        65536));
    }
  }
  row(&f, "flexmath", "every 3rd y, 5th x");
  row(&c, "CMSIS-DSP", "every 3rd y, 5th x");
}

static void atan2_q31(void) {
  ErrStats f, c;
  stats_init(&f, "atan2 q31", "2^-32 turn");
  stats_init(&c, "atan2 q31", "2^-32 turn");
  uint64_t st = 5U;
  for (int i = 0; i < 10000000; i++) {
    uint32_t u = test_rng_u32(&st);
    uint32_t v = test_rng_u32(&st);
    q31_t y = (q31_t)u >> (v & 31U);
    q31_t x = (q31_t)v >> (u & 31U);
    if (x == 0 && y == 0) {
      continue; /* CMSIS returns an error and no angle */
    }
    q31_t out;
    arm_atan2_q31(y, x, &out);
    double ref = atan2(y, x);
    stats_add(&f, y, ang_err(fm_atan2_q31(y, x), ref, 4294967296.0));
    stats_add(&c, y,
              ang_err(out / 536870912.0 * (4294967296.0 / 6.283185307179586),
                      ref, 4294967296.0));
  }
  row(&f, "flexmath", "10^7 random, every magnitude, not (0, 0)");
  row(&c, "CMSIS-DSP", "10^7 random, every magnitude, not (0, 0)");
}

/* natural log: ours is Q16.16 in and out; CMSIS takes q31 / q15 in (0, 1)
 * and gives q5.26 / q4.11, so each is in its own output's LSB */
static void ln_rows(void) {
  ErrStats f, c31, c15;
  stats_init(&f, "ln, Q16.16 out", "lsb");
  stats_init(&c31, "ln, q5.26 out", "lsb");
  stats_init(&c15, "ln, q4.11 out", "lsb");
  uint64_t st = 6U;
  for (int i = 0; i < 10000000; i++) {
    uint32_t u = test_rng_u32(&st);
    int32_t x = ((int32_t)(u >> 1) >> (u & 31U)) | 1;
    stats_add(&f, x, fm_ln_q16(x) - log(x / 65536.0) * 65536.0);
    q31_t out;
    arm_vlog_q31(&x, &out, 1U);
    stats_add(&c31, x, out - log(x / 2147483648.0) * 67108864.0);
  }
  for (int32_t x = 1; x < 32768; x++) {
    q15_t in = (q15_t)x, out;
    arm_vlog_q15(&in, &out, 1U);
    stats_add(&c15, x, out - log(x / 32768.0) * 2048.0);
  }
  row(&f, "flexmath", "10^7 random Q16.16, every magnitude");
  row(&c31, "CMSIS-DSP", "10^7 random q31, every magnitude");
  row(&c15, "CMSIS-DSP", "all 32767 positive q15");
}

/* float sin / cos in ULPs, and absolute error in units of 2^-24 (CMSIS
 * interpolates a table: good absolute error, poor near the zeros) */
static void sincos_f32(void) {
  static const char *const range[2] = {"10^7 random, x in [-pi, pi]",
                                       "10^7 random, abs(x) pi/4 .. 2^22"};
  for (int w = 0; w < 2; w++) {
    ErrStats st[8];
    const char *name[4] = {"sinf", "cosf", "sinf, absolute (2^-24)",
                           "cosf, absolute (2^-24)"};
    for (int k = 0; k < 8; k++) {
      stats_init(&st[k], name[k / 2], k < 4 ? "ulp" : "2^-24");
    }
    uint64_t rng = 7U;
    for (int i = 0; i < 10000000; i++) {
      uint32_t u = test_rng_u32(&rng);
      float x;
      if (w == 0) {
        x = (float)(int32_t)u * 1.46291807926716e-9f;
      } else {
        uint32_t b = (u << 31) | (0x3F490FDBU + (u >> 1) % 0x0AB6F025U);
        memcpy(&x, &b, sizeof x);
      }
      double ref[2] = {sin((double)x), cos((double)x)};
      float got[4] = {fm_sinf(x), arm_sin_f32(x), fm_cosf(x), arm_cos_f32(x)};
      for (int k = 0; k < 4; k++) {
        stats_add(&st[k], x, ulp_err_f32(got[k], ref[k / 2]));
        stats_add(&st[4 + k], x, ((double)got[k] - ref[k / 2]) * 16777216.0);
      }
    }
    for (int k = 0; k < 8; k++) {
      row(&st[k], k % 2 == 0 ? "flexmath" : "CMSIS-DSP", range[w]);
    }
  }
}

int main(void) {
  printf("# flexmath accuracy\n\n"
         "Generated by `make accuracy` on the host: each flexmath function and "
         "the\nCMSIS-DSP function it replaces (built from the same sources as "
         "the bench),\nboth against a `double` reference. Errors are in LSBs "
         "of the output, or\nULPs for float outputs; bias is the mean signed "
         "error. CMSIS-DSP is built with its\ndefault flags (no "
         "ARM_MATH_ROUNDING), as on the bench.\n\n"
         "| function | impl | inputs | max | mean | bias |\n"
         "|---|---|---|--:|--:|--:|\n");
  mul_q15();
  mul_q31();
  f32_to_q15();
  q31_to_f32();
  sqrt_q15();
  sqrt_q31();
  div_q15();
  div_q31();
  sin_q15();
  sin_q31();
  atan2_q15();
  atan2_q31();
  ln_rows();
  sincos_f32();
  return 0;
}
