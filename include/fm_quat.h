/**
 * @file fm_quat.h
 * @brief Float quaternions: product, normalise, rotate, integrate gyro rates
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * r may be the same object as an input. An inf or NaN component in any input
 * gives NaN components.
 */

#ifndef FM_QUAT_H
#define FM_QUAT_H

typedef struct {
  float w, x, y, z;
} FM_Quat;

/* a b (Hamilton); each component is its exact sum of products, rounded once */
void fm_quat_mul(FM_Quat *r, const FM_Quat *a, const FM_Quat *b);

/* q / |q| within 0.6 ULP per component, for any finite q; zero gives the
 * identity */
void fm_quat_normalize(FM_Quat *r, const FM_Quat *q);

/* q v q* for a unit q, within 2^-22 |v| per component */
void fm_quat_rotate(float r[3], const FM_Quat *q, const float v[3]);

/* q times the rotation by |w| dt about w (rad/s, body frame), exactly rather
 * than to first order, then normalised: within 2^-22 per component for a unit
 * q. |w|^2 must be finite (|w| < 1.8e19): beyond, NaN. */
void fm_quat_integrate(FM_Quat *r, const FM_Quat *q, const float w[3],
                       float dt);

#endif /* FM_QUAT_H */
