/**
 * @file fm_linalg.c
 * @brief Vectors and 3x3 / 4x4 matrices in q31 and float
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_linalg.h"
#include "fm_internal.h"
#include <string.h>

#define F32_QNAN 0x7FC00000U

/* ---- q31 ---- */

FX_INLINE fm_q31 dot_q31(const fm_q31 *a, const fm_q31 *b, uint32_t bs,
                         uint32_t n) {
  FM_SumQ31 acc = {0U, 0};
#pragma GCC unroll 4
  for (uint32_t i = 0U; i < n; i++) {
    fm_sum_mac(&acc, a[i], b[i * bs]);
  }
  return fm_sum_q31(acc);
}

/* a b - c d */
FX_INLINE fm_q31 diff_q31(fm_q31 a, fm_q31 b, fm_q31 c, fm_q31 d) {
  FM_SumQ31 acc = {0U, 0};
  fm_sum_mac(&acc, a, b);
  fm_sum_msub(&acc, c, d);
  return fm_sum_q31(acc);
}

fm_q31 fm_dot2_q31(const fm_q31 a[2], const fm_q31 b[2]) {
  return dot_q31(a, b, 1U, 2U);
}

fm_q31 fm_dot3_q31(const fm_q31 a[3], const fm_q31 b[3]) {
  return dot_q31(a, b, 1U, 3U);
}

fm_q31 fm_dot4_q31(const fm_q31 a[4], const fm_q31 b[4]) {
  return dot_q31(a, b, 1U, 4U);
}

fm_q31 fm_cross2_q31(const fm_q31 a[2], const fm_q31 b[2]) {
  return diff_q31(a[0], b[1], a[1], b[0]);
}

void fm_cross3_q31(fm_q31 r[3], const fm_q31 a[3], const fm_q31 b[3]) {
  r[0] = diff_q31(a[1], b[2], a[2], b[1]);
  r[1] = diff_q31(a[2], b[0], a[0], b[2]);
  r[2] = diff_q31(a[0], b[1], a[1], b[0]);
}

/* r = a b, a n x n, b n x bc */
FX_INLINE void mat_q31(fm_q31 *r, const fm_q31 *a, const fm_q31 *b, uint32_t n,
                       uint32_t bc) {
  for (uint32_t i = 0U; i < n; i++) {
    fm_q31 row[4]; /* in registers across the columns */
#pragma GCC unroll 4
    for (uint32_t k = 0U; k < n; k++) {
      row[k] = a[i * n + k];
    }
#pragma GCC unroll 4
    for (uint32_t j = 0U; j < bc; j++) {
      r[i * bc + j] = dot_q31(row, b + j, bc, n);
    }
  }
}

void fm_mat3_mul_q31(fm_q31 r[9], const fm_q31 a[9], const fm_q31 b[9]) {
  mat_q31(r, a, b, 3U, 3U);
}

void fm_mat3_vec_q31(fm_q31 r[3], const fm_q31 m[9], const fm_q31 v[3]) {
  mat_q31(r, m, v, 3U, 1U);
}

void fm_mat4_mul_q31(fm_q31 r[16], const fm_q31 a[16], const fm_q31 b[16]) {
  mat_q31(r, a, b, 4U, 4U);
}

void fm_mat4_vec_q31(fm_q31 r[4], const fm_q31 m[16], const fm_q31 v[4]) {
  mat_q31(r, m, v, 4U, 1U);
}

/* ---- float ---- */

FX_INLINE uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

FX_INLINE void put(float *r, uint32_t b) { memcpy(r, &b, sizeof b); }

/* sum of n products a[i] b[i bs], b negated where bit i of neg is set */
FX_COLD static uint32_t sum_any(const float *a, const float *b, uint32_t bs,
                                uint32_t n, uint32_t neg) {
  int32_t ma[4];
  int32_t ea[4];
  int32_t mb[4];
  int32_t eb[4];
  uint32_t bad = 0U;
  uint32_t nan = 0U;
  uint32_t inf = 0U; /* bit 0: +inf seen, bit 1: -inf */
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t ba = f2u(a[i]);
    uint32_t bb = f2u(b[i * bs]) ^ (((neg >> i) & 1U) << 31);
    uint32_t xa = ba & 0x7FFFFFFFU;
    uint32_t xb = bb & 0x7FFFFFFFU;
    if (xa > 0x7F800000U || xb > 0x7F800000U) {
      nan = 1U;
    } else if (xa == 0x7F800000U || xb == 0x7F800000U) {
      nan |= xa == 0U || xb == 0U ? 1U : 0U;
      inf |= 1U << ((ba ^ bb) >> 31);
    }
    ma[i] = fm_unp_f32(ba, 7U, &ea[i], &bad);
    mb[i] = fm_unp_f32(bb, 6U, &eb[i], &bad);
  }
  if (nan != 0U || inf == 3U) {
    return F32_QNAN;
  }
  if (inf != 0U) {
    return inf == 1U ? 0x7F800000U : 0xFF800000U;
  }
  return fm_sum_f32(ma, ea, mb, eb, 1U, n);
}

FX_INLINE uint32_t dot_f32(const float *a, const float *b, uint32_t bs,
                           uint32_t n, uint32_t neg) {
  int32_t ma[4];
  int32_t ea[4];
  int32_t mb[4];
  int32_t eb[4];
  uint32_t bad = 0U;
#pragma GCC unroll 4
  for (uint32_t i = 0U; i < n; i++) {
    ma[i] = fm_unp_f32(f2u(a[i]), 7U, &ea[i], &bad);
    mb[i] = fm_unp_f32(f2u(b[i * bs]) ^ (((neg >> i) & 1U) << 31), 6U, &eb[i],
                       &bad);
  }
  if (bad != 0U) {
    return sum_any(a, b, bs, n, neg);
  }
  return fm_sum_f32(ma, ea, mb, eb, 1U, n);
}

/* a b - c d */
FX_INLINE uint32_t diff_f32(float a, float b, float c, float d) {
  const float x[2] = {a, c};
  const float y[2] = {b, d};
  return dot_f32(x, y, 1U, 2U, 2U);
}

float fm_dot2_f32(const float a[2], const float b[2]) {
  uint32_t r = dot_f32(a, b, 1U, 2U, 0U);
  float f;
  memcpy(&f, &r, sizeof f);
  return f;
}

float fm_dot3_f32(const float a[3], const float b[3]) {
  uint32_t r = dot_f32(a, b, 1U, 3U, 0U);
  float f;
  memcpy(&f, &r, sizeof f);
  return f;
}

float fm_dot4_f32(const float a[4], const float b[4]) {
  uint32_t r = dot_f32(a, b, 1U, 4U, 0U);
  float f;
  memcpy(&f, &r, sizeof f);
  return f;
}

float fm_cross2_f32(const float a[2], const float b[2]) {
  uint32_t r = diff_f32(a[0], b[1], a[1], b[0]);
  float f;
  memcpy(&f, &r, sizeof f);
  return f;
}

void fm_cross3_f32(float r[3], const float a[3], const float b[3]) {
  put(&r[0], diff_f32(a[1], b[2], a[2], b[1]));
  put(&r[1], diff_f32(a[2], b[0], a[0], b[2]));
  put(&r[2], diff_f32(a[0], b[1], a[1], b[0]));
}

FX_COLD static void mat_any(float *r, const float *a, const float *b,
                            uint32_t n, uint32_t bc) {
  for (uint32_t i = 0U; i < n; i++) {
#pragma GCC unroll 4
    for (uint32_t j = 0U; j < bc; j++) {
      put(&r[i * bc + j], sum_any(a + i * n, b + j, bc, n, 0U));
    }
  }
}

/* r = a b, a n x n, b n x bc: every element unpacked once */
FX_INLINE void mat_f32(float *r, const float *a, const float *b, uint32_t n,
                       uint32_t bc) {
  int32_t am[16];
  int32_t ae[16];
  int32_t bm[16];
  int32_t be[16];
  uint32_t bad = 0U;
#pragma GCC unroll 4
  for (uint32_t k = 0U; k < n * n; k++) {
    am[k] = fm_unp_f32(f2u(a[k]), 7U, &ae[k], &bad);
  }
#pragma GCC unroll 4
  for (uint32_t k = 0U; k < n * bc; k++) {
    bm[k] = fm_unp_f32(f2u(b[k]), 6U, &be[k], &bad);
  }
  if (bad != 0U) {
    mat_any(r, a, b, n, bc);
    return;
  }
  for (uint32_t i = 0U; i < n; i++) {
#pragma GCC unroll 4
    for (uint32_t j = 0U; j < bc; j++) {
      put(&r[i * bc + j],
          fm_sum_f32(am + i * n, ae + i * n, bm + j, be + j, bc, n));
    }
  }
}

void fm_mat3_mul_f32(float r[9], const float a[9], const float b[9]) {
  mat_f32(r, a, b, 3U, 3U);
}

void fm_mat3_vec_f32(float r[3], const float m[9], const float v[3]) {
  mat_f32(r, m, v, 3U, 1U);
}

void fm_mat4_mul_f32(float r[16], const float a[16], const float b[16]) {
  mat_f32(r, a, b, 4U, 4U);
}

void fm_mat4_vec_f32(float r[4], const float m[16], const float v[4]) {
  mat_f32(r, m, v, 4U, 1U);
}
