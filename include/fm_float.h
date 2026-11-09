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

#endif /* FM_FLOAT_H */
