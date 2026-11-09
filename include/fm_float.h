/**
 * @file fm_float.h
 * @brief Float functions on integer internals, for cores without an FPU
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_FLOAT_H
#define FM_FLOAT_H

/* IEEE sqrt: correctly rounded on every input, subnormals included. -0 gives
 * -0, x < 0 the default NaN, and a NaN comes back quieted. */
float fm_sqrtf(float x);

/* 1/x, correctly rounded on every input, subnormal results included: 1/+-0
 * gives +-inf, 1/+-inf +-0, and a NaN comes back quieted */
float fm_recipf(float x);

/* 1/sqrt(x), correctly rounded on every input: +0 gives +inf, -0 -inf, +inf
 * +0, x < 0 the default NaN, and a NaN comes back quieted */
float fm_rsqrtf(float x);

/* sin, cos of x in radians, any finite x (exact reduction), within 0.515 ULP:
 * sin x = x and cos x = 1 for |x| < 2^-12. inf gives the default NaN, and a NaN
 * comes back quieted. fm_sincosf gives the same bits as the two calls. */
float fm_sinf(float x);
float fm_cosf(float x);
void fm_sincosf(float x, float *s, float *c);

#endif /* FM_FLOAT_H */
