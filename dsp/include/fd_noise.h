/**
 * @file fd_noise.h
 * @brief Uniform, Gaussian and triangular (dither) noise
 *
 * Each function draws from the fu_rand.h generator r.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_NOISE_H
#define FD_NOISE_H

#include "fm_fixed.h"
#include "fu_rand.h"
#include <stdint.h>

/** Uniform over the full scale: every value equally likely. */
void fd_noise_uniform_q15(FU_Xoshiro128 *r, fm_q15 *y, uint32_t n);
/** Uniform over the full scale: every value equally likely. */
void fd_noise_uniform_q31(FU_Xoshiro128 *r, fm_q31 *y, uint32_t n);

/** Gaussian, zero mean, standard deviation sigma, saturated; tails reach
 * 6.6 sigma (Box-Muller). */
void fd_noise_gauss_q15(FU_Xoshiro128 *r, fm_q15 sigma, fm_q15 *y, uint32_t n);
/** Gaussian, zero mean, standard deviation sigma, saturated; tails reach
 * 6.6 sigma (Box-Muller). */
void fd_noise_gauss_q31(FU_Xoshiro128 *r, fm_q31 sigma, fm_q31 *y, uint32_t n);

/** Triangular in (-2^bits, 2^bits) LSB, bits <= 15: dither before dropping
 * bits. */
void fd_noise_tpdf_q15(FU_Xoshiro128 *r, uint32_t bits, fm_q15 *y, uint32_t n);

#endif /* FD_NOISE_H */
