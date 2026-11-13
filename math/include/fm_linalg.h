/**
 * @file fm_linalg.h
 * @brief Vectors and 3x3 / 4x4 matrices in q31 and float
 *
 * Matrices are row-major arrays. Every output is one exact sum of products,
 * rounded once. `r` must not overlap an input.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_LINALG_H
#define FM_LINALG_H

#include "fm_fixed.h"

/**
 * @name q31
 * Rounded to nearest (ties up) and saturated: within 0.5 LSB.
 * @{
 */

/** a . b, 2 terms. */
fm_q31 fm_dot2_q31(const fm_q31 a[2], const fm_q31 b[2]);
/** a . b, 3 terms. */
fm_q31 fm_dot3_q31(const fm_q31 a[3], const fm_q31 b[3]);
/** a . b, 4 terms. */
fm_q31 fm_dot4_q31(const fm_q31 a[4], const fm_q31 b[4]);
/** a0 b1 - a1 b0. */
fm_q31 fm_cross2_q31(const fm_q31 a[2], const fm_q31 b[2]);
/** r = a x b. */
void fm_cross3_q31(fm_q31 r[3], const fm_q31 a[3], const fm_q31 b[3]);
/** r = a b, 3x3. */
void fm_mat3_mul_q31(fm_q31 r[9], const fm_q31 a[9], const fm_q31 b[9]);
/** r = m v, 3x3. */
void fm_mat3_vec_q31(fm_q31 r[3], const fm_q31 m[9], const fm_q31 v[3]);
/** r = a b, 4x4. */
void fm_mat4_mul_q31(fm_q31 r[16], const fm_q31 a[16], const fm_q31 b[16]);
/** r = m v, 4x4. */
void fm_mat4_vec_q31(fm_q31 r[4], const fm_q31 m[16], const fm_q31 v[4]);

/** @} */

/**
 * @name float
 * Within 0.5 ULP + 2^-32 sum |a_i b_i|; inf and NaN as IEEE.
 * @{
 */

/** a . b, 2 terms. */
float fm_dot2_f32(const float a[2], const float b[2]);
/** a . b, 3 terms. */
float fm_dot3_f32(const float a[3], const float b[3]);
/** a . b, 4 terms. */
float fm_dot4_f32(const float a[4], const float b[4]);
/** a0 b1 - a1 b0. */
float fm_cross2_f32(const float a[2], const float b[2]);
/** r = a x b. */
void fm_cross3_f32(float r[3], const float a[3], const float b[3]);
/** r = a b, 3x3. */
void fm_mat3_mul_f32(float r[9], const float a[9], const float b[9]);
/** r = m v, 3x3. */
void fm_mat3_vec_f32(float r[3], const float m[9], const float v[3]);
/** r = a b, 4x4. */
void fm_mat4_mul_f32(float r[16], const float a[16], const float b[16]);
/** r = m v, 4x4. */
void fm_mat4_vec_f32(float r[4], const float m[16], const float v[4]);

/** @} */

#endif /* FM_LINALG_H */
