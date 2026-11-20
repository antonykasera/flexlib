/**
 * @file fd_bfp.h
 * @brief float blocks <-> q31 mantissas with one block exponent (internal)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_BFP_H
#define FD_BFP_H

#include <stdint.h>

/* fd_bfp_exp's result for a block with a NaN or an infinity */
#define FD_BFP_BAD INT32_MAX

/* e with every |x| < 2^e (-125 for a block of zeros), or FD_BFP_BAD */
int32_t fd_bfp_exp(const float *x, uint32_t n);

/* q = x 2^(31 - e), nearest, ties to even; needs every |x| < 2^e, finite.
 * Exact for x within 2^7 of 2^e. q may be x's storage. */
void fd_bfp_to_q31(const float *x, int32_t *q, uint32_t n, int32_t e);

/* y = q 2^(e - 31), nearest, ties to even; overflow gives infinity. y may be
 * q's storage. */
void fd_bfp_to_f32(const int32_t *q, float *y, uint32_t n, int32_t e);

#endif /* FD_BFP_H */
