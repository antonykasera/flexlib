/**
 * @file cases_linalg.c
 * @brief Bench cases for fm_linalg.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fm_internal.h"
#include "fm_linalg.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

/* ---- data: operands from two pools, the result in out ---- */

#define POOL 80U
static fm_q31 qpool[POOL];
static float fpool[POOL];
static fm_q31 qout[16];
static float fout[16];

static void pools_init(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  uint32_t s = 0x9E3779B9U;
  for (unsigned i = 0U; i < POOL; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    qpool[i] = (fm_q31)s >> (s & 3U);
    /* +-[2^-4, 2^4) */
    uint32_t b = (s & 0x807FFFFFU) | ((123U + (s >> 23) % 8U) << 23);
    memcpy(&fpool[i], &b, sizeof b);
  }
}

/* operand offsets: a at x, b at 63 - x */
static inline uint32_t in_off(uint32_t u) {
  pools_init();
  return u % 64U;
}

#define QA (qpool + x)
#define QB (qpool + 63U - x)
#define FA (fpool + x)
#define FB (fpool + 63U - x)
#define QOUT(n) h = bench_hash(h, qout, (n) * 4U)
#define FOUT(n) h = bench_hash(h, fout, (n) * 4U)

/* ---- candidates ---- */

/* mat4 q31 with SMLAL into int64: wraps when |sum| >= 2 */
BENCH_CALL static void mat4_q31_smlal(fm_q31 *r, const fm_q31 *a,
                                      const fm_q31 *b) {
  for (unsigned i = 0U; i < 4U; i++) {
    for (unsigned j = 0U; j < 4U; j++) {
      int64_t s = 0;
#pragma GCC unroll 4
      for (unsigned k = 0U; k < 4U; k++) {
        s += (int64_t)a[4U * i + k] * b[4U * k + j];
      }
      int64_t q = (s + (1LL << 30)) >> 31;
      r[4U * i + j] =
          q > INT32_MAX ? INT32_MAX : (q < INT32_MIN ? INT32_MIN : (fm_q31)q);
    }
  }
}

/* mat4 float, each sum unpacking its own operands (specials: the library) */
BENCH_CALL static void mat4_f32_per_sum(float *r, const float *a,
                                        const float *b) {
  for (unsigned i = 0U; i < 4U; i++) {
    for (unsigned j = 0U; j < 4U; j++) {
      int32_t ma[4];
      int32_t ea[4];
      int32_t mb[4];
      int32_t eb[4];
      uint32_t bad = 0U;
#pragma GCC unroll 4
      for (unsigned k = 0U; k < 4U; k++) {
        uint32_t x;
        uint32_t y;
        memcpy(&x, &a[4U * i + k], sizeof x);
        memcpy(&y, &b[4U * k + j], sizeof y);
        ma[k] = unp_f32(x, 7U, &ea[k], &bad);
        mb[k] = unp_f32(y, 6U, &eb[k], &bad);
      }
      if (bad != 0U) {
        fm_mat4_mul_f32(r, a, b);
        return;
      }
      uint32_t s = sum_f32(ma, ea, mb, eb, 1U, 4U);
      memcpy(&r[4U * i + j], &s, sizeof s);
    }
  }
}

/* ---- plain C (float: libgcc soft-float) ---- */

BENCH_CALL static float dot3_plain(const float *a, const float *b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

BENCH_CALL static void cross3_plain(float *r, const float *a, const float *b) {
  r[0] = a[1] * b[2] - a[2] * b[1];
  r[1] = a[2] * b[0] - a[0] * b[2];
  r[2] = a[0] * b[1] - a[1] * b[0];
}

BENCH_CALL static void mat_plain(float *r, const float *a, const float *b,
                                 unsigned n) {
  for (unsigned i = 0U; i < n; i++) {
    for (unsigned j = 0U; j < n; j++) {
      float s = 0.0f;
      for (unsigned k = 0U; k < n; k++) {
        s += a[n * i + k] * b[n * k + j];
      }
      r[n * i + j] = s;
    }
  }
}

#ifdef BENCH_HAVE_CMSIS_DSP
BENCH_CALL static void mat4_cmsis_q31(fm_q31 *r, fm_q31 *a, fm_q31 *b) {
  arm_matrix_instance_q31 ma = {4U, 4U, a};
  arm_matrix_instance_q31 mb = {4U, 4U, b};
  arm_matrix_instance_q31 mr = {4U, 4U, r};
  (void)arm_mat_mult_q31(&ma, &mb, &mr);
}

BENCH_CALL static void mat4_cmsis_f32(float *r, float *a, float *b) {
  arm_matrix_instance_f32 ma = {4U, 4U, a};
  arm_matrix_instance_f32 mb = {4U, 4U, b};
  arm_matrix_instance_f32 mr = {4U, 4U, r};
  (void)arm_mat_mult_f32(&ma, &mb, &mr);
}
#endif

/* ---- cases ---- */

BENCH_UNARY(l_dot3_q31, uint32_t, in_off, fm_q31, fm_dot3_q31(QA, QB))
BENCH_UNARY_POST(l_cross3_q31, uint32_t, in_off, uint32_t,
                 (fm_cross3_q31(qout, QA, QB), 0U), QOUT(3U))
BENCH_UNARY_POST(l_m3v_q31, uint32_t, in_off, uint32_t,
                 (fm_mat3_vec_q31(qout, QA, QB), 0U), QOUT(3U))
BENCH_UNARY_POST(l_m3_q31, uint32_t, in_off, uint32_t,
                 (fm_mat3_mul_q31(qout, QA, QB), 0U), QOUT(9U))
BENCH_UNARY_POST(l_m4v_q31, uint32_t, in_off, uint32_t,
                 (fm_mat4_vec_q31(qout, QA, QB), 0U), QOUT(4U))
BENCH_UNARY_POST(l_m4_q31, uint32_t, in_off, uint32_t,
                 (fm_mat4_mul_q31(qout, QA, QB), 0U), QOUT(16U))
BENCH_UNARY_POST(l_m4_q31_smlal, uint32_t, in_off, uint32_t,
                 (mat4_q31_smlal(qout, QA, QB), 0U), QOUT(16U))
BENCH_UNARY(l_dot3_f32, uint32_t, in_off, float, fm_dot3_f32(FA, FB))
BENCH_UNARY(l_dot3_plain, uint32_t, in_off, float, dot3_plain(FA, FB))
BENCH_UNARY_POST(l_cross3_f32, uint32_t, in_off, uint32_t,
                 (fm_cross3_f32(fout, FA, FB), 0U), FOUT(3U))
BENCH_UNARY_POST(l_cross3_plain, uint32_t, in_off, uint32_t,
                 (cross3_plain(fout, FA, FB), 0U), FOUT(3U))
BENCH_UNARY_POST(l_m3v_f32, uint32_t, in_off, uint32_t,
                 (fm_mat3_vec_f32(fout, FA, FB), 0U), FOUT(3U))
BENCH_UNARY_POST(l_m3_f32, uint32_t, in_off, uint32_t,
                 (fm_mat3_mul_f32(fout, FA, FB), 0U), FOUT(9U))
BENCH_UNARY_POST(l_m3_plain, uint32_t, in_off, uint32_t,
                 (mat_plain(fout, FA, FB, 3U), 0U), FOUT(9U))
BENCH_UNARY_POST(l_m4v_f32, uint32_t, in_off, uint32_t,
                 (fm_mat4_vec_f32(fout, FA, FB), 0U), FOUT(4U))
BENCH_UNARY_POST(l_m4_f32, uint32_t, in_off, uint32_t,
                 (fm_mat4_mul_f32(fout, FA, FB), 0U), FOUT(16U))
BENCH_UNARY_POST(l_m4_f32_per_sum, uint32_t, in_off, uint32_t,
                 (mat4_f32_per_sum(fout, FA, FB), 0U), FOUT(16U))
BENCH_UNARY_POST(l_m4_plain, uint32_t, in_off, uint32_t,
                 (mat_plain(fout, FA, FB, 4U), 0U), FOUT(16U))
BENCH_UNARY_DEF(l_m4_q31_cold, uint32_t, in_off, uint32_t,
                (fm_mat4_mul_q31(qout, QA, QB), 0U), QOUT(16U),
                bench_cold_flush())
BENCH_UNARY_DEF(l_m4_f32_cold, uint32_t, in_off, uint32_t,
                (fm_mat4_mul_f32(fout, FA, FB), 0U), FOUT(16U),
                bench_cold_flush())
#ifdef BENCH_HAVE_CMSIS_DSP
BENCH_UNARY_POST(l_m4_cmsis_q31, uint32_t, in_off, uint32_t,
                 (mat4_cmsis_q31(qout, QA, QB), 0U), QOUT(16U))
BENCH_UNARY_POST(l_m4_cmsis_f32, uint32_t, in_off, uint32_t,
                 (mat4_cmsis_f32(fout, FA, FB), 0U), FOUT(16U))
#endif

static const BenchCase cases[] = {
    {"fm_dot3_q31", "flexmath", l_dot3_q31, true},
    {"fm_cross3_q31", "flexmath", l_cross3_q31, true},
    {"fm_mat3_vec_q31", "flexmath", l_m3v_q31, true},
    {"fm_mat3_mul_q31", "flexmath", l_m3_q31, true},
    {"fm_mat4_vec_q31", "flexmath", l_m4v_q31, true},
    {"fm_mat4_mul_q31", "flexmath", l_m4_q31, true},
    {"mat4 q31, SMLAL (wraps for |sum| >= 2)", "candidate", l_m4_q31_smlal,
     true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_mat_mult_q31, 4x4", "CMSIS-DSP", l_m4_cmsis_q31, false},
#endif
    {"fm_dot3_f32", "flexmath", l_dot3_f32, true},
    {"dot3, float loop", "libgcc", l_dot3_plain, true},
    {"fm_cross3_f32", "flexmath", l_cross3_f32, true},
    {"cross3, float", "libgcc", l_cross3_plain, true},
    {"fm_mat3_vec_f32", "flexmath", l_m3v_f32, true},
    {"fm_mat3_mul_f32", "flexmath", l_m3_f32, true},
    {"mat3 mul, float loop", "libgcc", l_m3_plain, true},
    {"fm_mat4_vec_f32", "flexmath", l_m4v_f32, true},
    {"fm_mat4_mul_f32", "flexmath", l_m4_f32, true},
    {"mat4 f32, unpacking inside each sum", "candidate", l_m4_f32_per_sum,
     true},
    {"mat4 mul, float loop", "libgcc", l_m4_plain, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_mat_mult_f32, 4x4", "CMSIS-DSP", l_m4_cmsis_f32, false},
#endif
};

const BenchGroup bench_group_linalg = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fm_mat4_mul_q31", "flexmath", l_m4_q31_cold, true},
    {"fm_mat4_mul_f32", "flexmath", l_m4_f32_cold, true},
};

const BenchGroup bench_group_linalg_cold = {cold, BENCH_COUNT(cold)};
