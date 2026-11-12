/**
 * @file fm_float.h
 * @brief Float functions on integer internals, for cores without an FPU
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_FLOAT_H
#define FM_FLOAT_H

/* correctly rounded; x < 0 gives the default NaN, NaNs are quieted */
float fm_sqrtf(float x);

/* correctly rounded, subnormals included; 1/+-0 gives +-inf */
float fm_recipf(float x);

/* correctly rounded; +-0 gives +-inf, x < 0 the default NaN */
float fm_rsqrtf(float x);

/* radians, any finite x, within 0.515 ULP; inf gives the default NaN */
float fm_sinf(float x);
float fm_cosf(float x);
void fm_sincosf(float x, float *s, float *c);

/* (-pi, pi] within 0.545 ULP, C special cases */
float fm_atan2f(float y, float x);

/* within 0.545 ULP; |x| > 1 gives the default NaN */
float fm_asinf(float x);
float fm_acosf(float x);

/* within 0.52 ULP; +-0 gives -inf, x < 0 the default NaN */
float fm_log2f(float x);
float fm_logf(float x);

/* within 0.52 ULP; overflow gives +inf, underflow +0 */
float fm_exp2f(float x);
float fm_expf(float x);

#endif /* FM_FLOAT_H */
