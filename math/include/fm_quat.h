/**
 * @file fm_quat.h
 * @brief Float quaternions: product, normalise, rotate, integrate gyro rates
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_QUAT_H
#define FM_QUAT_H

/** Quaternion $w + x i + y j + z k$. */
typedef struct {
  float w; /**< scalar part */
  float x; /**< i */
  float y; /**< j */
  float z; /**< k */
} FM_Quat;

/** $r = a b$ (Hamilton), each component within 0.5 ULP + 2^-32 sum of the
 * |products|; r may alias an input; inf or NaN in gives NaN. */
void fm_quat_mul(FM_Quat *r, const FM_Quat *a, const FM_Quat *b);

/** $r = q / |q|$ within 0.6 ULP, any finite q; zero gives the identity. */
void fm_quat_normalize(FM_Quat *r, const FM_Quat *q);

/** $r = q v q*$ for a unit q, within $2^-22 |v|$. */
void fm_quat_rotate(float r[3], const FM_Quat *q, const float v[3]);

/** Rotate q by the body rates w (rad/s) over dt: the exact rotation,
 * renormalised; within 2^-22 for a unit q. */
void fm_quat_integrate(FM_Quat *r, const FM_Quat *q, const float w[3],
                       float dt);

#endif /* FM_QUAT_H */
