/**
 * @file fd_vec_f32.c
 * @brief Float element-wise operations
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_vec.h"
#include <string.h>

void fd_add_f32(const float *a, const float *b, float *y, uint32_t n) {
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = a[i] + b[i];
  }
}

void fd_sub_f32(const float *a, const float *b, float *y, uint32_t n) {
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = a[i] - b[i];
  }
}

void fd_mul_f32(const float *a, const float *b, float *y, uint32_t n) {
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = a[i] * b[i];
  }
}

void fd_scale_f32(const float *x, float k, float *y, uint32_t n) {
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = x[i] * k;
  }
}

void fd_offset_f32(const float *x, float c, float *y, uint32_t n) {
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = x[i] + c;
  }
}

/* sign bit only: exact, NaN kept */
FX_INLINE void bits_op(const float *x, float *y, uint32_t n, uint32_t and_m,
                       uint32_t xor_m) {
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t b;
    memcpy(&b, &x[i], sizeof b);
    b = (b & and_m) ^ xor_m;
    memcpy(&y[i], &b, sizeof b);
  }
}

void fd_negate_f32(const float *x, float *y, uint32_t n) {
  bits_op(x, y, n, 0xFFFFFFFFU, 0x80000000U);
}

void fd_abs_f32(const float *x, float *y, uint32_t n) {
  bits_op(x, y, n, 0x7FFFFFFFU, 0U);
}

void fd_clip_f32(const float *x, float lo, float hi, float *y, uint32_t n) {
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    float v = x[i];
    y[i] = v < lo ? lo : (v > hi ? hi : v);
  }
}

void fd_fill_f32(float c, float *y, uint32_t n) {
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = c;
  }
}

void fd_copy_f32(const float *x, float *y, uint32_t n) {
  bits_op(x, y, n, 0xFFFFFFFFU, 0U);
}
