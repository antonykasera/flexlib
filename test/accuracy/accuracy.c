/**
 * @file accuracy.c
 * @brief `make accuracy`
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "arm_const_structs.h"
#include "arm_math.h"
#include "fd_biquad.h"
#include "fd_fft.h"
#include "fd_fir.h"
#include "fd_vec.h"
#include "fm_atan.h"
#include "fm_div.h"
#include "fm_exp.h"
#include "fm_fixed.h"
#include "fm_float.h"
#include "fm_linalg.h"
#include "fm_quat.h"
#include "fm_sqrt.h"
#include "fm_trig.h"
#include "ref_dsp.h"
#include "test_stats.h"
#include <stdio.h>
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

/* CMSIS returns a quotient and a shift */
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

/* angle error in 2^-bits turns, mod a turn */
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
      stats_add(
          &c, y,
          ang_err(out / 8192.0 * (65536 / 6.283185307179586), ref, 65536));
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

/* natural log: ours is Q16.16 in and out */
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

/* float atan2 in ULPs and absolute, on +-1 pairs and on any magnitudes */
static void atan2_f32(void) {
  static const char *const range[2] = {"10^7 random pairs in [-1, 1]",
                                       "10^7 random pairs, any magnitudes"};
  for (int w = 0; w < 2; w++) {
    ErrStats st[4];
    stats_init(&st[0], "atan2f", "ulp");
    stats_init(&st[1], "atan2f", "ulp");
    stats_init(&st[2], "atan2f, absolute (2^-24)", "2^-24");
    stats_init(&st[3], "atan2f, absolute (2^-24)", "2^-24");
    uint64_t rng = 9U;
    for (int i = 0; i < 10000000; i++) {
      uint32_t u = test_rng_u32(&rng);
      uint32_t v = test_rng_u32(&rng);
      float y;
      float x;
      if (w == 0) {
        y = (float)(int32_t)u * 4.65661287e-10f;
        x = (float)(int32_t)v * 4.65661287e-10f;
      } else {
        u = (u & 0x80FFFFFFU) | ((u >> 1) % 0x7E000000U & 0x7F000000U);
        v = (v & 0x80FFFFFFU) | ((v >> 1) % 0x7E000000U & 0x7F000000U);
        memcpy(&y, &u, sizeof y);
        memcpy(&x, &v, sizeof x);
      }
      double ref = atan2((double)y, (double)x);
      float got[2];
      got[0] = fm_atan2f(y, x);
      (void)arm_atan2_f32(y, x, &got[1]);
      for (int k = 0; k < 2; k++) {
        stats_add(&st[k], y, ulp_err_f32(got[k], ref));
        stats_add(&st[2 + k], y, ((double)got[k] - ref) * 16777216.0);
      }
    }
    for (int k = 0; k < 4; k++) {
      row(&st[k], k % 2 == 0 ? "flexmath" : "CMSIS-DSP", range[w]);
    }
  }
}

static void mat4_rows(void) {
  __extension__ typedef __int128 i128;
  ErrStats st[6];
  stats_init(&st[0], "4x4 multiply q31", "lsb");
  stats_init(&st[1], "4x4 multiply q31", "lsb");
  for (int k = 2; k < 6; k++) {
    stats_init(&st[k], "4x4 multiply f32", "ulp");
  }
  uint64_t rng = 11U;
  for (int t = 0; t < 200000; t++) {
    q31_t qa[16], qb[16], qr[2][16];
    float fa[16], fb[16], fr[2][16];
    for (int i = 0; i < 16; i++) {
      qa[i] = (q31_t)test_rng_u32(&rng) >> 2;
      qb[i] = (q31_t)test_rng_u32(&rng) >> 2;
      uint32_t u = test_rng_u32(&rng);
      uint32_t v = test_rng_u32(&rng);
      u = (u & 0x807FFFFFU) | ((123U + (u >> 23) % 8U) << 23);
      v = (v & 0x807FFFFFU) | ((123U + (v >> 23) % 8U) << 23);
      memcpy(&fa[i], &u, sizeof u);
      memcpy(&fb[i], &v, sizeof v);
    }
    if ((t & 1) != 0) { /* row 0 . */
      fa[3] = -(fa[0] * fb[0] + fa[1] * fb[4] + fa[2] * fb[8]) / fb[12];
    }
    arm_matrix_instance_q31 ma = {4U, 4U, qa}, mb = {4U, 4U, qb};
    arm_matrix_instance_q31 mr = {4U, 4U, qr[1]};
    arm_matrix_instance_f32 fma = {4U, 4U, fa}, fmb = {4U, 4U, fb};
    arm_matrix_instance_f32 fmr = {4U, 4U, fr[1]};
    (void)arm_mat_mult_q31(&ma, &mb, &mr);
    (void)arm_mat_mult_f32(&fma, &fmb, &fmr);
    fm_mat4_mul_q31(qr[0], qa, qb);
    fm_mat4_mul_f32(fr[0], fa, fb);
    for (int i = 0; i < 16; i++) {
      i128 sq = 0;
      long double sf = 0.0L;
      for (int k = 0; k < 4; k++) {
        sq += (i128)qa[i / 4 * 4 + k] * qb[k * 4 + i % 4];
        sf += (long double)fa[i / 4 * 4 + k] * (long double)fb[k * 4 + i % 4];
      }
      double exact = (double)((long double)sq / 2147483648.0L);
      for (int k = 0; k < 2; k++) {
        stats_add(&st[k], exact, qr[k][i] - exact);
        stats_add(&st[2 + 2 * (t & 1) + k], (double)sf,
                  ulp_err_f32(fr[k][i], (double)sf));
      }
    }
  }
  row(&st[0], "flexmath", "200000 random matrices in [-1/4, 1/4)");
  row(&st[1], "CMSIS-DSP", "200000 random matrices in [-1/4, 1/4)");
  row(&st[2], "flexmath", "100000 random matrices");
  row(&st[3], "CMSIS-DSP", "100000 random matrices");
  row(&st[4], "flexmath", "100000 with row 0 . column 0 nearly cancelling");
  row(&st[5], "CMSIS-DSP", "100000 with row 0 . column 0 nearly cancelling");
}

/* quaternion product and normalisation, components +-[2^-4, 2^4) */
static void quat_rows(void) {
  ErrStats st[4];
  stats_init(&st[0], "quaternion product f32", "ulp");
  stats_init(&st[1], "quaternion product f32", "ulp");
  stats_init(&st[2], "quaternion normalize f32", "ulp");
  stats_init(&st[3], "quaternion normalize f32", "ulp");
  uint64_t rng = 13U;
  for (int t = 0; t < 1000000; t++) {
    float a[4], b[4], r[2][4], n[2][4];
    long double la[4], lb[4];
    for (int i = 0; i < 4; i++) {
      uint32_t u = test_rng_u32(&rng);
      uint32_t v = test_rng_u32(&rng);
      u = (u & 0x807FFFFFU) | ((123U + (u >> 23) % 8U) << 23);
      v = (v & 0x807FFFFFU) | ((123U + (v >> 23) % 8U) << 23);
      memcpy(&a[i], &u, sizeof u);
      memcpy(&b[i], &v, sizeof v);
      la[i] = (long double)a[i];
      lb[i] = (long double)b[i];
    }
    long double p[4] = {
        la[0] * lb[0] - la[1] * lb[1] - la[2] * lb[2] - la[3] * lb[3],
        la[0] * lb[1] + la[1] * lb[0] + la[2] * lb[3] - la[3] * lb[2],
        la[0] * lb[2] - la[1] * lb[3] + la[2] * lb[0] + la[3] * lb[1],
        la[0] * lb[3] + la[1] * lb[2] - la[2] * lb[1] + la[3] * lb[0]};
    long double nn =
        sqrtl(la[0] * la[0] + la[1] * la[1] + la[2] * la[2] + la[3] * la[3]);
    FM_Quat qa, qb, qr, qn;
    memcpy(&qa, a, sizeof qa);
    memcpy(&qb, b, sizeof qb);
    fm_quat_mul(&qr, &qa, &qb);
    fm_quat_normalize(&qn, &qa);
    memcpy(r[0], &qr, sizeof qr);
    memcpy(n[0], &qn, sizeof qn);
    arm_quaternion_product_single_f32(a, b, r[1]);
    arm_quaternion_normalize_f32(a, n[1], 1U);
    for (int i = 0; i < 4; i++) {
      for (int k = 0; k < 2; k++) {
        stats_add(&st[k], (double)p[i], ulp_err_f32(r[k][i], (double)p[i]));
        stats_add(&st[2 + k], (double)nn,
                  ulp_err_f32(n[k][i], (double)(la[i] / nn)));
      }
    }
  }
  for (int k = 0; k < 4; k++) {
    row(&st[k], k % 2 == 0 ? "flexmath" : "CMSIS-DSP",
        "10^6 random quaternions");
  }
}

/* ---- flexdsp: blocks of 64 ---- */

__extension__ typedef __int128 i128;
#define BLK 64U
#define NBLK 100000

static void fill_q15(uint64_t *st, q15_t *v, unsigned n) {
  for (unsigned i = 0U; i < n; i++) {
    v[i] = (q15_t)(test_rng_u32(st) >> 16);
  }
}

static void fill_q31(uint64_t *st, q31_t *v, unsigned n) {
  for (unsigned i = 0U; i < n; i++) {
    v[i] = (q31_t)test_rng_u32(st);
  }
}

static double clampd(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static void fd_scale_rows(void) {
  ErrStats f, c;
  stats_init(&f, "scale q15", "lsb");
  stats_init(&c, "scale q15", "lsb");
  uint64_t st = 21U;
  q15_t x[BLK], yf[BLK], yc[BLK];
  for (int b = 0; b < NBLK; b++) {
    fill_q15(&st, x, BLK);
    q15_t k = (q15_t)(test_rng_u32(&st) >> 16);
    fd_scale_q15(x, k, 0, yf, BLK);
    arm_scale_q15(x, k, 0, yc, BLK);
    for (unsigned i = 0U; i < BLK; i++) {
      double ref = clampd((double)x[i] * k / 32768.0, -32768.0, 32767.0);
      stats_add(&f, x[i], yf[i] - ref);
      stats_add(&c, x[i], yc[i] - ref);
    }
  }
  row(&f, "flexdsp", "10^5 blocks of 64, random k");
  row(&c, "CMSIS-DSP", "10^5 blocks of 64, random k");
}

static void fd_q31_to_q15_rows(void) {
  ErrStats f, c;
  stats_init(&f, "q31 to q15", "lsb");
  stats_init(&c, "q31 to q15", "lsb");
  uint64_t st = 22U;
  q31_t x[BLK];
  q15_t yf[BLK], yc[BLK];
  for (int b = 0; b < NBLK; b++) {
    fill_q31(&st, x, BLK);
    fd_q31_to_q15(x, yf, BLK);
    arm_q31_to_q15(x, yc, BLK);
    for (unsigned i = 0U; i < BLK; i++) {
      double ref = clampd(x[i] / 65536.0, -32768.0, 32767.0);
      stats_add(&f, x[i], yf[i] - ref);
      stats_add(&c, x[i], yc[i] - ref);
    }
  }
  row(&f, "flexdsp", "10^5 blocks of 64");
  row(&c, "CMSIS-DSP", "10^5 blocks of 64");
}

static void fd_dot_rows(void) {
  ErrStats f, c, pf, pc;
  stats_init(&f, "dot q31, Q16.48 out", "lsb");
  stats_init(&c, "dot q31, Q16.48 out", "lsb");
  stats_init(&pf, "power q31, Q16.48 out", "lsb");
  stats_init(&pc, "power q31, Q16.48 out", "lsb");
  uint64_t st = 23U;
  q31_t a[BLK], bb[BLK];
  for (int b = 0; b < NBLK; b++) {
    fill_q31(&st, a, BLK);
    fill_q31(&st, bb, BLK);
    i128 s = 0, p = 0;
    for (unsigned i = 0U; i < BLK; i++) {
      s += (i128)a[i] * bb[i];
      p += (i128)a[i] * a[i];
    }
    q63_t rc, prc;
    arm_dot_prod_q31(a, bb, BLK, &rc);
    arm_power_q31(a, BLK, &prc);
    int64_t rf = fd_dot_q31(a, bb, BLK);
    uint64_t prf = fd_power_q31(a, BLK);
    stats_add(&f, b, (double)((i128)rf * 16384 - s) / 16384.0);
    stats_add(&c, b, (double)((i128)rc * 16384 - s) / 16384.0);
    stats_add(&pf, b, (double)((i128)prf * 16384 - p) / 16384.0);
    stats_add(&pc, b, (double)((i128)prc * 16384 - p) / 16384.0);
  }
  row(&f, "flexdsp", "10^5 blocks of 64");
  row(&c, "CMSIS-DSP", "10^5 blocks of 64");
  row(&pf, "flexdsp", "10^5 blocks of 64");
  row(&pc, "CMSIS-DSP", "10^5 blocks of 64");
}

static void fd_moment_rows(void) {
  ErrStats rf, rc, vf, vc, sf, sc;
  stats_init(&rf, "rms q15", "lsb");
  stats_init(&rc, "rms q15", "lsb");
  stats_init(&vf, "var q15", "lsb");
  stats_init(&vc, "var q15", "lsb");
  stats_init(&sf, "std q15", "lsb");
  stats_init(&sc, "std q15", "lsb");
  uint64_t st = 24U;
  q15_t x[BLK];
  for (int b = 0; b < NBLK; b++) {
    fill_q15(&st, x, BLK);
    unsigned sh = test_rng_u32(&st) % 8U; /* every amplitude */
    double s1 = 0.0, s2 = 0.0;
    for (unsigned i = 0U; i < BLK; i++) {
      x[i] = (q15_t)(x[i] >> sh);
      s1 += x[i];
      s2 += (double)x[i] * x[i];
    }
    double rms = sqrt(s2 / BLK);
    double var = (BLK * s2 - s1 * s1) / ((double)BLK * (BLK - 1U)) / 32768.0;
    double sd = sqrt((BLK * s2 - s1 * s1) / ((double)BLK * (BLK - 1U)));
    q15_t c;
    arm_rms_q15(x, BLK, &c);
    stats_add(&rf, b, fd_rms_q15(x, BLK) - clampd(rms, 0.0, 32767.0));
    stats_add(&rc, b, c - clampd(rms, 0.0, 32767.0));
    arm_var_q15(x, BLK, &c);
    stats_add(&vf, b, fd_var_q15(x, BLK) - clampd(var, 0.0, 32767.0));
    stats_add(&vc, b, c - clampd(var, 0.0, 32767.0));
    arm_std_q15(x, BLK, &c);
    stats_add(&sf, b, fd_std_q15(x, BLK) - clampd(sd, 0.0, 32767.0));
    stats_add(&sc, b, c - clampd(sd, 0.0, 32767.0));
  }
  row(&rf, "flexdsp", "10^5 blocks of 64, every amplitude");
  row(&rc, "CMSIS-DSP", "10^5 blocks of 64, every amplitude");
  row(&vf, "flexdsp", "10^5 blocks of 64, every amplitude");
  row(&vc, "CMSIS-DSP", "10^5 blocks of 64, every amplitude");
  row(&sf, "flexdsp", "10^5 blocks of 64, every amplitude");
  row(&sc, "CMSIS-DSP", "10^5 blocks of 64, every amplitude");
}

static void fd_complex_rows(void) {
  ErrStats mf, mc, cf, cc;
  stats_init(&mf, "complex magnitude q15, UQ1.15 out", "lsb");
  stats_init(&mc, "complex magnitude q15, Q2.14 out", "lsb");
  stats_init(&cf, "complex multiply q15, q15 out", "lsb");
  stats_init(&cc, "complex multiply q15, Q3.13 out", "lsb");
  uint64_t st = 25U;
  fd_cq15 a[BLK], bb[BLK], yf[BLK];
  q15_t yc[2U * BLK];
  uint16_t m[BLK];
  for (int b = 0; b < NBLK; b++) {
    fill_q15(&st, (q15_t *)a, 2U * BLK);
    fill_q15(&st, (q15_t *)bb, 2U * BLK);
    fd_cmag_q15(a, m, BLK);
    arm_cmplx_mag_q15((const q15_t *)a, yc, BLK);
    for (unsigned i = 0U; i < BLK; i++) {
      double ref = sqrt((double)a[i].re * a[i].re + (double)a[i].im * a[i].im);
      stats_add(&mf, b, m[i] - ref);
      stats_add(&mc, b, yc[i] - ref / 2.0);
    }
    fd_cmul_q15(a, bb, yf, BLK);
    arm_cmplx_mult_cmplx_q15((const q15_t *)a, (const q15_t *)bb, yc, BLK);
    for (unsigned i = 0U; i < BLK; i++) {
      double re = ((double)a[i].re * bb[i].re - (double)a[i].im * bb[i].im);
      double im = ((double)a[i].re * bb[i].im + (double)a[i].im * bb[i].re);
      stats_add(&cf, b, yf[i].re - clampd(re / 32768.0, -32768.0, 32767.0));
      stats_add(&cf, b, yf[i].im - clampd(im / 32768.0, -32768.0, 32767.0));
      stats_add(&cc, b, yc[2U * i] - re / 131072.0);
      stats_add(&cc, b, yc[2U * i + 1U] - im / 131072.0);
    }
  }
  row(&mf, "flexdsp", "10^5 blocks of 64");
  row(&mc, "CMSIS-DSP", "10^5 blocks of 64");
  row(&cf, "flexdsp", "10^5 blocks of 64");
  row(&cc, "CMSIS-DSP", "10^5 blocks of 64");
}

static void fd_fir_rows(void) {
  enum { T = 32, N = 64, NB = 15625 }; /* 10^6 samples */
  ErrStats f15, c15, cf15, f31, c31, cf31;
  stats_init(&f15, "FIR q15, 32 taps", "lsb");
  stats_init(&c15, "FIR q15, 32 taps", "lsb");
  stats_init(&cf15, "FIR q15, 32 taps (fast)", "lsb");
  stats_init(&f31, "FIR q31, 32 taps", "lsb");
  stats_init(&c31, "FIR q31, 32 taps", "lsb");
  stats_init(&cf31, "FIR q31, 32 taps (fast)", "lsb");
  float hf[T];
  q15_t h15[T], r15[T];
  q31_t h31[T], r31[T];
  (void)fd_fir_design(hf, T, FD_LOWPASS, 0.1f, 0.0f, FD_WIN_HAMMING);
  (void)fd_fir_quantize_q15(hf, h15, T);
  (void)fd_fir_quantize_q31(hf, h31, T);
  for (int k = 0; k < T; k++) { /* CMSIS stores them time-reversed */
    r15[k] = h15[T - 1 - k];
    r31[k] = h31[T - 1 - k];
  }
  static q15_t s15[T - 1 + N], cs15[T + N], cfs15[T + N];
  static q31_t s31[T - 1 + N], cs31[T + N], cfs31[T + N];
  FD_FirQ15 a15;
  FD_FirQ31 a31;
  arm_fir_instance_q15 i15, if15;
  arm_fir_instance_q31 i31, if31;
  fd_fir_init_q15(&a15, h15, T, s15, T - 1 + N);
  fd_fir_init_q31(&a31, h31, T, s31, T - 1 + N);
  (void)arm_fir_init_q15(&i15, T, r15, cs15, N);
  (void)arm_fir_init_q15(&if15, T, r15, cfs15, N);
  arm_fir_init_q31(&i31, T, r31, cs31, N);
  arm_fir_init_q31(&if31, T, r31, cfs31, N);
  static q15_t hx15[T - 1 + N];
  static q31_t hx31[T - 1 + N];
  uint64_t st = 26U;
  for (int b = 0; b < NB; b++) {
    q15_t x15[N], y15[N], yc15[N], ycf15[N];
    q31_t x31[N], y31[N], yc31[N], ycf31[N];
    fill_q15(&st, x15, N);
    fill_q31(&st, x31, N);
    fd_fir_q15(&a15, x15, y15, N);
    arm_fir_q15(&i15, x15, yc15, N);
    arm_fir_fast_q15(&if15, x15, ycf15, N);
    fd_fir_q31(&a31, x31, y31, N);
    arm_fir_q31(&i31, x31, yc31, N);
    arm_fir_fast_q31(&if31, x31, ycf31, N);
    memmove(hx15, hx15 + N, (T - 1) * sizeof hx15[0]);
    memmove(hx31, hx31 + N, (T - 1) * sizeof hx31[0]);
    memcpy(hx15 + T - 1, x15, sizeof x15);
    memcpy(hx31 + T - 1, x31, sizeof x31);
    for (int i = 0; i < N; i++) {
      i128 s = 0, S = 0;
      for (int k = 0; k < T; k++) {
        s += (i128)h15[k] * hx15[i + T - 1 - k];
        S += (i128)h31[k] * hx31[i + T - 1 - k];
      }
      double r = clampd((double)s / 32768.0, -32768.0, 32767.0);
      double R = clampd((double)S / 2147483648.0, -2147483648.0, 2147483647.0);
      stats_add(&f15, i, y15[i] - r);
      stats_add(&c15, i, yc15[i] - r);
      stats_add(&cf15, i, ycf15[i] - r);
      stats_add(&f31, i, y31[i] - R);
      stats_add(&c31, i, yc31[i] - R);
      stats_add(&cf31, i, ycf31[i] - R);
    }
  }
  row(&f15, "flexdsp", "10^6 random samples, Hamming lowpass");
  row(&c15, "CMSIS-DSP", "10^6 random samples, Hamming lowpass");
  row(&cf15, "CMSIS-DSP", "10^6 random samples, Hamming lowpass");
  row(&f31, "flexdsp", "10^6 random samples, Hamming lowpass");
  row(&c31, "CMSIS-DSP", "10^6 random samples, Hamming lowpass");
  row(&cf31, "CMSIS-DSP", "10^6 random samples, Hamming lowpass");
}

/* 4th-order Butterworth lowpass, 10^6 samples of uniform noise at half scale,
 * against the same quantised coefficients in double */
static void fd_biquad_rows(float fc, const char *label, int q15_rows) {
  enum { NS = 2, N = 64, NB = 15625 };
  static char names[7][64];
  ErrStats e[7];
  const char *impl[7] = {"flexdsp",         "flexdsp error feedback",
                         "CMSIS-DSP",       "CMSIS-DSP fast",
                         "CMSIS-DSP 32x64", "flexdsp",
                         "CMSIS-DSP"};
  for (int i = 0; i < 7; i++) {
    snprintf(names[i], sizeof names[i], "biquad %s, %s", i < 5 ? "q31" : "q15",
             label);
    stats_init(&e[i], names[i], "lsb");
  }
  FD_BiquadDesign d[NS];
  fm_q31 c31[5 * NS];
  fm_q15 c15[5 * NS];
  q15_t cc15[6 * NS];
  uint32_t sh31, sh15;
  (void)fd_biquad_butter(d, 2U * NS, FD_LOWPASS, fc);
  (void)fd_biquad_quantize_q31(d, NS, c31, &sh31);
  (void)fd_biquad_quantize_q15(d, NS, c15, &sh15);
  for (int k = 0; k < NS; k++) {
    const fm_q15 *c = c15 + 5 * k;
    q15_t t[6] = {c[0], 0, c[1], c[2], c[3], c[4]};
    memcpy(cc15 + 6 * k, t, sizeof t);
  }
  static fm_q31 s0[FD_BIQUAD_STATE_LEN(NS)], s1[FD_BIQUAD_STATE_LEN(NS)];
  static fm_q15 s5[FD_BIQUAD_STATE_LEN(NS)];
  static q31_t cs31[4 * NS], cfs31[4 * NS];
  static q63_t cs64[4 * NS];
  static q15_t cs15[4 * NS];
  FD_BiquadQ31 f0, f1;
  FD_BiquadQ15 f5;
  arm_biquad_casd_df1_inst_q31 i31, if31;
  arm_biquad_cas_df1_32x64_ins_q31 i64;
  arm_biquad_casd_df1_inst_q15 i15;
  fd_biquad_init_q31(&f0, c31, NS, sh31, s0);
  fd_biquad_init_ef_q31(&f1, c31, NS, sh31, s1);
  fd_biquad_init_q15(&f5, c15, NS, sh15, s5);
  memset(cs31, 0, sizeof cs31);
  memset(cfs31, 0, sizeof cfs31);
  memset(cs64, 0, sizeof cs64);
  memset(cs15, 0, sizeof cs15);
  arm_biquad_cascade_df1_init_q31(&i31, NS, c31, cs31, 1);
  arm_biquad_cascade_df1_init_q31(&if31, NS, c31, cfs31, 1);
  arm_biquad_cas_df1_32x64_init_q31(&i64, NS, c31, cs64, 1);
  arm_biquad_cascade_df1_init_q15(&i15, NS, cc15, cs15, 1);
  double h31[4 * NS] = {0}, h15[4 * NS] = {0}, cf31[5 * NS], cf15[5 * NS];
  for (int i = 0; i < 5 * NS; i++) {
    cf31[i] = c31[i] / 1073741824.0;
    cf15[i] = c15[i] / 16384.0;
  }
  uint64_t st = 27U;
  for (int b = 0; b < NB; b++) {
    q31_t x[N], y[5][N];
    q15_t x15[N], y15[2][N];
    for (int i = 0; i < N; i++) {
      x[i] = (q31_t)test_rng_u32(&st) >> 1;
      x15[i] = (q15_t)(x[i] >> 16);
    }
    fd_biquad_q31(&f0, x, y[0], N);
    fd_biquad_q31(&f1, x, y[1], N);
    arm_biquad_cascade_df1_q31(&i31, x, y[2], N);
    arm_biquad_cascade_df1_fast_q31(&if31, x, y[3], N);
    arm_biquad_cas_df1_32x64_q31(&i64, x, y[4], N);
    fd_biquad_q15(&f5, x15, y15[0], N);
    arm_biquad_cascade_df1_q15(&i15, x15, y15[1], N);
    for (int i = 0; i < N; i++) {
      double v = x[i], w = x15[i];
      for (int k = 0; k < NS; k++) {
        const double *q = cf31 + 5 * k, *r = cf15 + 5 * k;
        double *g = h31 + 4 * k, *u = h15 + 4 * k;
        double o =
            q[0] * v + q[1] * g[0] + q[2] * g[1] + q[3] * g[2] + q[4] * g[3];
        double p =
            r[0] * w + r[1] * u[0] + r[2] * u[1] + r[3] * u[2] + r[4] * u[3];
        g[1] = g[0], g[0] = v, g[3] = g[2], g[2] = o, v = o;
        u[1] = u[0], u[0] = w, u[3] = u[2], u[2] = p, w = p;
      }
      if (b > 10) { /* past the start-up */
        for (int j = 0; j < 5; j++) {
          stats_add(&e[j], i, y[j][i] - v);
        }
        stats_add(&e[5], i, y15[0][i] - w);
        stats_add(&e[6], i, y15[1][i] - w);
      }
    }
  }
  for (int i = 0; i < (q15_rows ? 7 : 5); i++) {
    row(&e[i], impl[i], "10^6 samples of half-scale noise");
  }
}

/* ---- transforms: error energy and the peak error, both against the
 * largest bin, as the output formats differ (block exponent vs X / n) ---- */

typedef struct {
  double sig, err, peak, worst; /* sums of squares; largest |X|, |error| */
} FftErr;

static void fft_add(FftErr *e, double got_re, double got_im, double re,
                    double im) {
  double dr = got_re - re, di = got_im - im;
  e->sig += re * re + im * im;
  e->err += dr * dr + di * di;
  e->peak = fmax(e->peak, fmax(fabs(re), fabs(im)));
  e->worst = fmax(e->worst, fmax(fabs(dr), fabs(di)));
}

static void fft_row(const char *name, const char *impl, unsigned n,
                    const char *input, const FftErr *e) {
  printf("| %s | %s | %u | %s | %.1f | %.1f |\n", name, impl, n, input,
         10.0 * log10(e->sig / e->err), 20.0 * log10(e->peak / e->worst));
}

/* kind 0 white, 1 tone (off-bin), 2 two tones, 3 white at -40 dB */
static void fft_input(uint64_t *st, int kind, double full, unsigned len,
                      double *v) {
  double f1 = 3.0 + (double)(test_rng_u32(st) % 50U) + 0.37;
  double f2 = f1 * 2.71 + 5.2;
  for (unsigned i = 0U; i < len; i++) {
    double r = (double)(int32_t)test_rng_u32(st) / 2147483648.0;
    double t = 6.283185307179586 * (double)(i / 2U) / (len / 2U);
    double a = (i & 1U) == 0U ? cos(f1 * t) : sin(f1 * t);
    double b = (i & 1U) == 0U ? cos(f2 * t) : -sin(f2 * t);
    double x = kind == 0
                   ? r
                   : (kind == 1 ? a : (kind == 2 ? 0.5 * (a + b) : r / 100.0));
    v[i] = fmin(fmax(floor(x * full + 0.5), -full), full - 1.0);
  }
}

static void fd_fft_rows(void) {
  static const char *const kinds[4] = {"white", "tone", "two tones",
                                       "white, -40 dB"};
  static const unsigned sizes[3] = {64U, 256U, 1024U};
  enum { REPS = 40 };
  static double v[2048], re[1024], im[1024];
  static fd_cq15 a15[1024];
  static fd_cq31 a31[1024];
  static q15_t c15[2048];
  static q31_t c31[2048];
  for (unsigned si = 0U; si < 3U; si++) {
    unsigned n = sizes[si];
    const arm_cfft_instance_q15 *i15 =
        n == 64U
            ? &arm_cfft_sR_q15_len64
            : (n == 256U ? &arm_cfft_sR_q15_len256 : &arm_cfft_sR_q15_len1024);
    const arm_cfft_instance_q31 *i31 =
        n == 64U
            ? &arm_cfft_sR_q31_len64
            : (n == 256U ? &arm_cfft_sR_q31_len256 : &arm_cfft_sR_q31_len1024);
    for (int kind = 0; kind < 4; kind++) {
      FftErr e[4];
      memset(e, 0, sizeof e);
      uint64_t st = 99U + (uint64_t)kind;
      for (int rep = 0; rep < REPS; rep++) {
        for (int q31 = 0; q31 <= 1; q31++) {
          double full = q31 ? 2147483648.0 : 32768.0;
          fft_input(&st, kind, full, 2U * n, v);
          for (unsigned i = 0U; i < n; i++) {
            re[i] = v[2U * i];
            im[i] = v[2U * i + 1U];
          }
          ref_fft(re, im, n, 0);
          for (unsigned i = 0U; i < 2U * n; i++) {
            if (q31) {
              c31[i] = (q31_t)v[i];
            } else {
              c15[i] = (q15_t)v[i];
            }
          }
          for (unsigned i = 0U; i < n; i++) {
            if (q31) {
              a31[i].re = c31[2U * i];
              a31[i].im = c31[2U * i + 1U];
            } else {
              a15[i].re = c15[2U * i];
              a15[i].im = c15[2U * i + 1U];
            }
          }
          int32_t x = q31 ? fd_cfft_q31(a31, n) : fd_cfft_q15(a15, n);
          if (q31) {
            arm_cfft_q31(i31, c31, 0U, 1U);
          } else {
            arm_cfft_q15(i15, c15, 0U, 1U);
          }
          double u = ldexp(1.0, x);
          for (unsigned k = 0U; k < n; k++) {
            double fr = q31 ? a31[k].re : a15[k].re;
            double fi = q31 ? a31[k].im : a15[k].im;
            double cr = q31 ? c31[2U * k] : c15[2U * k];
            double ci = q31 ? c31[2U * k + 1U] : c15[2U * k + 1U];
            fft_add(&e[2 * q31], fr * u, fi * u, re[k], im[k]);
            fft_add(&e[2 * q31 + 1], cr * n, ci * n, re[k], im[k]);
          }
        }
      }
      fft_row("cfft q15", "flexdsp", n, kinds[kind], &e[0]);
      fft_row("cfft q15", "CMSIS-DSP", n, kinds[kind], &e[1]);
      fft_row("cfft q31", "flexdsp", n, kinds[kind], &e[2]);
      fft_row("cfft q31", "CMSIS-DSP", n, kinds[kind], &e[3]);
    }
  }
  /* real transforms, n = 512 */
  static q15_t rs15[512], rd15[1024];
  static q31_t rs31[512], rd31[1024];
  static fm_q15 r15[512] __attribute__((aligned(4)));
  static fm_q31 r31[512];
  arm_rfft_instance_q15 ri15;
  arm_rfft_instance_q31 ri31;
  (void)arm_rfft_init_512_q15(&ri15, 0U, 1U);
  (void)arm_rfft_init_512_q31(&ri31, 0U, 1U);
  for (int kind = 0; kind < 4; kind++) {
    FftErr e[4];
    memset(e, 0, sizeof e);
    uint64_t st = 199U + (uint64_t)kind;
    for (int rep = 0; rep < REPS; rep++) {
      for (int q31 = 0; q31 <= 1; q31++) {
        double full = q31 ? 2147483648.0 : 32768.0;
        fft_input(&st, kind, full, 512U, v);
        for (unsigned i = 0U; i < 512U; i++) {
          re[i] = v[i];
          im[i] = 0.0;
          if (q31) {
            r31[i] = rs31[i] = (q31_t)v[i];
          } else {
            r15[i] = rs15[i] = (q15_t)v[i];
          }
        }
        ref_fft(re, im, 512U, 0);
        int32_t x = q31 ? fd_rfft_q31(r31, 512U) : fd_rfft_q15(r15, 512U);
        if (q31) {
          arm_rfft_q31(&ri31, rs31, rd31);
        } else {
          arm_rfft_q15(&ri15, rs15, rd15);
        }
        double u = ldexp(1.0, x);
        for (unsigned k = 0U; k <= 256U; k++) {
          double fr, fi;
          if (k == 0U || k == 256U) {
            fr = (q31 ? r31[k == 0U ? 0U : 1U] : r15[k == 0U ? 0U : 1U]) * u;
            fi = 0.0;
          } else {
            fr = (q31 ? r31[2U * k] : r15[2U * k]) * u;
            fi = (q31 ? r31[2U * k + 1U] : r15[2U * k + 1U]) * u;
          }
          double cr = (q31 ? rd31[2U * k] : rd15[2U * k]) * 512.0;
          double ci = (q31 ? rd31[2U * k + 1U] : rd15[2U * k + 1U]) * 512.0;
          fft_add(&e[2 * q31], fr, fi, re[k], im[k]);
          fft_add(&e[2 * q31 + 1], cr, ci, re[k], im[k]);
        }
      }
    }
    fft_row("rfft q15", "flexdsp", 512U, kinds[kind], &e[0]);
    fft_row("rfft q15", "CMSIS-DSP", 512U, kinds[kind], &e[1]);
    fft_row("rfft q31", "flexdsp", 512U, kinds[kind], &e[2]);
    fft_row("rfft q31", "CMSIS-DSP", 512U, kinds[kind], &e[3]);
  }
}

int main(void) {
  printf(
      "# flexlib accuracy\n\n"
      "Generated by `make accuracy` on the host: each flexmath and flexdsp "
      "function\nand the CMSIS-DSP function it replaces (built from the same "
      "sources as the\nbench), both against a `double` or exact reference. "
      "Errors are in LSBs of each\nfunction's own output format, or ULPs for "
      "float outputs; bias is the mean signed\nerror. CMSIS-DSP is built "
      "with its default flags (no ARM_MATH_ROUNDING), as on\nthe bench.\n\n"
      "## flexmath\n\n"
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
  atan2_f32();
  mat4_rows();
  quat_rows();
  printf("\n## flexdsp\n\n"
         "| function | impl | inputs | max | mean | bias |\n"
         "|---|---|---|--:|--:|--:|\n");
  fd_scale_rows();
  fd_q31_to_q15_rows();
  fd_dot_rows();
  fd_moment_rows();
  fd_complex_rows();
  fd_fir_rows();
  fd_biquad_rows(0.01f, "Butterworth 4, fc 0.01", 1);
  fd_biquad_rows(0.0005f, "Butterworth 4, fc 5e-4", 0); /* Q2.14 b round to 0 */
  printf("\n## flexdsp transforms\n\n"
         "Signal-to-noise against the exact DFT, and the largest bin over the "
         "largest\nerror, both in dB (the output formats differ: a block "
         "exponent here, X / n\nin CMSIS-DSP). %d blocks per row.\n\n"
         "| transform | impl | n | input | SNR dB | peak / max error dB |\n"
         "|---|---|--:|---|--:|--:|\n",
         40);
  fd_fft_rows();
  return 0;
}
