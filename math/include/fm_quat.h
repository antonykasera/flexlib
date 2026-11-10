/**
 * @file fm_quat.h
 * @brief Float quaternions: product, normalise, rotate, integrate gyro rates
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_QUAT_H
#define FM_QUAT_H

typedef struct {
  float w, x, y, z;
} FM_Quat;

/* a b (Hamilton); r may alias an input; inf or NaN in gives NaN */
void fm_quat_mul(FM_Quat *r, const FM_Quat *a, const FM_Quat *b);

/* q / |q| within 0.6 ULP; zero gives the identity */
void fm_quat_normalize(FM_Quat *r, const FM_Quat *q);

/* q v q* for unit q, within 2^-22 |v| */
void fm_quat_rotate(float r[3], const FM_Quat *q, const float v[3]);

/* rotate q by w dt (rad/s, body frame), within 2^-22 for unit q */
void fm_quat_integrate(FM_Quat *r, const FM_Quat *q, const float w[3],
                       float dt);

#endif /* FM_QUAT_H */
