/**
 * @file fd_noise.h
 * @brief Uniform, Gaussian and triangular (dither) noise
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FD_NOISE_H
#define FD_NOISE_H

#include "fm_fixed.h"
#include "fu_rand.h"
#include <stdint.h>

/* every value equally likely (full scale) */
void fd_noise_uniform_q15(FU_Xoshiro128 *r, fm_q15 *y, uint32_t n);
void fd_noise_uniform_q31(FU_Xoshiro128 *r, fm_q31 *y, uint32_t n);

/* zero mean, standard deviation sigma, saturated (Box-Muller, tails to 6.6
 * sigma) */
void fd_noise_gauss_q15(FU_Xoshiro128 *r, fm_q15 sigma, fm_q15 *y, uint32_t n);
void fd_noise_gauss_q31(FU_Xoshiro128 *r, fm_q31 sigma, fm_q31 *y, uint32_t n);

/* triangular in (-2^bits, 2^bits) LSB, bits <= 15: dither for dropping bits */
void fd_noise_tpdf_q15(FU_Xoshiro128 *r, uint32_t bits, fm_q15 *y, uint32_t n);

#endif /* FD_NOISE_H */
