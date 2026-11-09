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

#endif /* FM_FLOAT_H */
