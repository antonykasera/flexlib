/**
 * @file fd_vec.h
 * @brief Block arithmetic, statistics, conversions and complex vectors
 *
 * Each function works on n elements. Results round to nearest (ties up) and
 * saturate, within 0.5 LSB, unless stated. `y` may equal an input of the
 * same type.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_VEC_H
#define FD_VEC_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/**
 * @name Element-wise
 * @{
 */

/** y = a + b. */
void fd_add_q15(const fm_q15 *a, const fm_q15 *b, fm_q15 *y, uint32_t n);
/** y = a + b. */
void fd_add_q31(const fm_q31 *a, const fm_q31 *b, fm_q31 *y, uint32_t n);
/** y = a - b. */
void fd_sub_q15(const fm_q15 *a, const fm_q15 *b, fm_q15 *y, uint32_t n);
/** y = a - b. */
void fd_sub_q31(const fm_q31 *a, const fm_q31 *b, fm_q31 *y, uint32_t n);
/** y = a b. */
void fd_mul_q15(const fm_q15 *a, const fm_q15 *b, fm_q15 *y, uint32_t n);
/** y = a b. */
void fd_mul_q31(const fm_q31 *a, const fm_q31 *b, fm_q31 *y, uint32_t n);

/** y = x k 2^shift, shift -15 to 15. */
void fd_scale_q15(const fm_q15 *x, fm_q15 k, int32_t shift, fm_q15 *y,
                  uint32_t n);
/** y = x k 2^shift, shift -31 to 31. */
void fd_scale_q31(const fm_q31 *x, fm_q31 k, int32_t shift, fm_q31 *y,
                  uint32_t n);

/** y = x 2^shift, shift -15 to 15. */
void fd_shift_q15(const fm_q15 *x, int32_t shift, fm_q15 *y, uint32_t n);
/** y = x 2^shift, shift -31 to 31. */
void fd_shift_q31(const fm_q31 *x, int32_t shift, fm_q31 *y, uint32_t n);

/** y = x + c. */
void fd_offset_q15(const fm_q15 *x, fm_q15 c, fm_q15 *y, uint32_t n);
/** y = x + c. */
void fd_offset_q31(const fm_q31 *x, fm_q31 c, fm_q31 *y, uint32_t n);
/** y = -x. */
void fd_negate_q15(const fm_q15 *x, fm_q15 *y, uint32_t n);
/** y = -x. */
void fd_negate_q31(const fm_q31 *x, fm_q31 *y, uint32_t n);
/** y = |x|. */
void fd_abs_q15(const fm_q15 *x, fm_q15 *y, uint32_t n);
/** y = |x|. */
void fd_abs_q31(const fm_q31 *x, fm_q31 *y, uint32_t n);
/** x clamped to [lo, hi], lo <= hi. */
void fd_clip_q15(const fm_q15 *x, fm_q15 lo, fm_q15 hi, fm_q15 *y, uint32_t n);
/** x clamped to [lo, hi], lo <= hi. */
void fd_clip_q31(const fm_q31 *x, fm_q31 lo, fm_q31 hi, fm_q31 *y, uint32_t n);
/** Set n elements to c. */
void fd_fill_q15(fm_q15 c, fm_q15 *y, uint32_t n);
/** Set n elements to c. */
void fd_fill_q31(fm_q31 c, fm_q31 *y, uint32_t n);
/** Copy n elements. */
void fd_copy_q15(const fm_q15 *x, fm_q15 *y, uint32_t n);
/** Copy n elements. */
void fd_copy_q31(const fm_q31 *x, fm_q31 *y, uint32_t n);

/** @} */

/**
 * @name Conversions
 * As the fm_fixed.h scalars: from float to nearest even, NaN gives 0.
 * @{
 */

/** q15 to q31, exact. */
void fd_q15_to_q31(const fm_q15 *x, fm_q31 *y, uint32_t n);
/** q31 to q15. */
void fd_q31_to_q15(const fm_q31 *x, fm_q15 *y, uint32_t n);
/** q7 to q15, exact. */
void fd_q7_to_q15(const int8_t *x, fm_q15 *y, uint32_t n);
/** q15 to q7. */
void fd_q15_to_q7(const fm_q15 *x, int8_t *y, uint32_t n);
/** q15 to float, exact. */
void fd_q15_to_f32(const fm_q15 *x, float *y, uint32_t n);
/** float to q15. */
void fd_f32_to_q15(const float *x, fm_q15 *y, uint32_t n);
/** q31 to float, to nearest even. */
void fd_q31_to_f32(const fm_q31 *x, float *y, uint32_t n);
/** float to q31. */
void fd_f32_to_q31(const float *x, fm_q31 *y, uint32_t n);

/** 20 log10 x in Q16.16, 0 dB at full scale, within 0.54 LSB; x <= 0 gives
 * `INT32_MIN`. */
void fd_db_q15(const fm_q15 *x, fm_q16 *y, uint32_t n);
/** 20 log10 x in Q16.16, 0 dB at full scale, within 0.97 LSB; x <= 0 gives
 * `INT32_MIN`. */
void fd_db_q31(const fm_q31 *x, fm_q16 *y, uint32_t n);

/** @} */

/**
 * @name Statistics
 * @{
 */

/** Sum, exact. */
int64_t fd_sum_q15(const fm_q15 *x, uint32_t n);
/** Sum, exact. */
int64_t fd_sum_q31(const fm_q31 *x, uint32_t n);
/** Mean, n >= 1. */
fm_q15 fd_mean_q15(const fm_q15 *x, uint32_t n);
/** Mean, n >= 1. */
fm_q31 fd_mean_q31(const fm_q31 *x, uint32_t n);

/** Sum of a b, exact, Q30. */
int64_t fd_dot_q15(const fm_q15 *a, const fm_q15 *b, uint32_t n);
/** Sum of a b in Q16.48 (the CMSIS-DSP format), saturated past +-2^15. */
int64_t fd_dot_q31(const fm_q31 *a, const fm_q31 *b, uint32_t n);
/** Sum of x^2, exact, Q30. */
uint64_t fd_power_q15(const fm_q15 *x, uint32_t n);
/** Sum of x^2 in Q16.48, saturated past 2^16. */
uint64_t fd_power_q31(const fm_q31 *x, uint32_t n);

/** Root mean square, correctly rounded; n >= 1. */
fm_q15 fd_rms_q15(const fm_q15 *x, uint32_t n);
/** Root mean square, correctly rounded; n >= 1. */
fm_q31 fd_rms_q31(const fm_q31 *x, uint32_t n);
/** Variance over n - 1, correctly rounded; 2 <= n < 2^24. */
fm_q15 fd_var_q15(const fm_q15 *x, uint32_t n);
/** Variance over n - 1, correctly rounded; 2 <= n < 2^24. */
fm_q31 fd_var_q31(const fm_q31 *x, uint32_t n);
/** Standard deviation over n - 1, correctly rounded; 2 <= n < 2^24. */
fm_q15 fd_std_q15(const fm_q15 *x, uint32_t n);
/** Standard deviation over n - 1, correctly rounded; 2 <= n < 2^24. */
fm_q31 fd_std_q31(const fm_q31 *x, uint32_t n);

/** Spare sign bits h: every value fits 16 - h bits. */
uint32_t fd_headroom_q15(const fm_q15 *x, uint32_t n);
/** Spare sign bits h: every value fits 32 - h bits. */
uint32_t fd_headroom_q31(const fm_q31 *x, uint32_t n);

/** Least value, n >= 1; index (may be NULL) gets its first position. */
fm_q15 fd_min_q15(const fm_q15 *x, uint32_t n, uint32_t *index);
/** Least value, n >= 1; index (may be NULL) gets its first position. */
fm_q31 fd_min_q31(const fm_q31 *x, uint32_t n, uint32_t *index);
/** Greatest value, n >= 1; index (may be NULL) gets its first position. */
fm_q15 fd_max_q15(const fm_q15 *x, uint32_t n, uint32_t *index);
/** Greatest value, n >= 1; index (may be NULL) gets its first position. */
fm_q31 fd_max_q31(const fm_q31 *x, uint32_t n, uint32_t *index);
/** Greatest |x|, saturated, n >= 1; index (may be NULL) gets its first
 * position. */
fm_q15 fd_absmax_q15(const fm_q15 *x, uint32_t n, uint32_t *index);
/** Greatest |x|, saturated, n >= 1; index (may be NULL) gets its first
 * position. */
fm_q31 fd_absmax_q31(const fm_q31 *x, uint32_t n, uint32_t *index);
/** Least and greatest value, n >= 1. */
void fd_minmax_q15(const fm_q15 *x, uint32_t n, fm_q15 *min, fm_q15 *max);
/** Least and greatest value, n >= 1. */
void fd_minmax_q31(const fm_q31 *x, uint32_t n, fm_q31 *min, fm_q31 *max);

/** @} */

/**
 * @name Complex vectors
 * @{
 */

/** y = a b. */
void fd_cmul_q15(const fd_cq15 *a, const fd_cq15 *b, fd_cq15 *y, uint32_t n);
/** y = a b. */
void fd_cmul_q31(const fd_cq31 *a, const fd_cq31 *b, fd_cq31 *y, uint32_t n);
/** y = a b, b real. */
void fd_cmul_real_q15(const fd_cq15 *a, const fm_q15 *b, fd_cq15 *y,
                      uint32_t n);
/** y = a b, b real. */
void fd_cmul_real_q31(const fd_cq31 *a, const fm_q31 *b, fd_cq31 *y,
                      uint32_t n);
/** Complex conjugate. */
void fd_conj_q15(const fd_cq15 *x, fd_cq15 *y, uint32_t n);
/** Complex conjugate. */
void fd_conj_q31(const fd_cq31 *x, fd_cq31 *y, uint32_t n);

/** |x| in UQ1.15; never saturates. */
void fd_cmag_q15(const fd_cq15 *x, uint16_t *y, uint32_t n);
/** |x| in UQ1.31; never saturates. */
void fd_cmag_q31(const fd_cq31 *x, uint32_t *y, uint32_t n);

/** |x|^2 in UQ2.30, exact. */
void fd_cmag2_q15(const fd_cq15 *x, uint32_t *y, uint32_t n);
/** |x|^2 in UQ1.31. */
void fd_cmag2_q31(const fd_cq31 *x, uint32_t *y, uint32_t n);

/** Sum of a b, not conjugated; exact, Q30. */
void fd_cdot_q15(const fd_cq15 *a, const fd_cq15 *b, uint32_t n, int64_t *re,
                 int64_t *im);
/** Sum of a b, not conjugated; Q16.48, saturated past +-2^15. */
void fd_cdot_q31(const fd_cq31 *a, const fd_cq31 *b, uint32_t n, int64_t *re,
                 int64_t *im);

/** Pair n real and n imaginary parts. */
void fd_interleave_q15(const fm_q15 *re, const fm_q15 *im, fd_cq15 *y,
                       uint32_t n);
/** Pair n real and n imaginary parts. */
void fd_interleave_q31(const fm_q31 *re, const fm_q31 *im, fd_cq31 *y,
                       uint32_t n);
/** Split n complex values into parts. */
void fd_deinterleave_q15(const fd_cq15 *x, fm_q15 *re, fm_q15 *im, uint32_t n);
/** Split n complex values into parts. */
void fd_deinterleave_q31(const fd_cq31 *x, fm_q31 *re, fm_q31 *im, uint32_t n);

/** @} */

#endif /* FD_VEC_H */
