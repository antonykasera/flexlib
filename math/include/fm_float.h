/**
 * @file fm_float.h
 * @brief Float functions on integer internals, for cores without an FPU
 *
 * Integer operations only. Subnormals are handled; a NaN input comes back
 * quieted with its payload. Errors are in ULP of the result.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_FLOAT_H
#define FM_FLOAT_H

/**
 * @name Roots and reciprocals
 * Correctly rounded.
 * @{
 */

/** $sqrt(x)$; $x < 0$ gives the default NaN. */
float fm_sqrtf(float x);

/** $1 / x$, subnormals included; $+-0$ gives $+-inf$. */
float fm_recipf(float x);

/** $1 / sqrt(x)$; $+-0$ gives $+-inf$, $x < 0$ the default NaN. */
float fm_rsqrtf(float x);

/** @} */

/**
 * @name Trigonometry
 * In radians.
 * @{
 */

/** $sin(x)$ for any finite x, within 0.515 ULP; inf gives the default NaN. */
float fm_sinf(float x);
/** $cos(x)$ for any finite x, within 0.515 ULP; inf gives the default NaN. */
float fm_cosf(float x);
/** fm_sinf() and fm_cosf() in one call, the same bits. */
void fm_sincosf(float x, float *s, float *c);

/** $atan2(y, x)$ in $(-pi, pi]$, within 0.545 ULP; zeros and infinities as
 * C's atan2f(). */
float fm_atan2f(float y, float x);

/** $asin(x)$, within 0.545 ULP; $|x| > 1$ gives the default NaN. */
float fm_asinf(float x);
/** $acos(x)$, within 0.545 ULP; $|x| > 1$ gives the default NaN. */
float fm_acosf(float x);

/** @} */

/**
 * @name Logarithms and exponentials
 * Within 0.52 ULP.
 * @{
 */

/** $log2(x)$; $+-0$ gives $-inf$, $x < 0$ the default NaN. */
float fm_log2f(float x);
/** $ln(x)$; $+-0$ gives $-inf$, $x < 0$ the default NaN. */
float fm_logf(float x);

/** $2^x$; overflow gives $+inf$, underflow $+0$. */
float fm_exp2f(float x);
/** $e^x$; overflow gives $+inf$, underflow $+0$. */
float fm_expf(float x);

/** @} */

#endif /* FM_FLOAT_H */
