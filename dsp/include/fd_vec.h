/**
 * @file fd_vec.h
 * @brief Block arithmetic, statistics, conversions and complex vectors
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FD_VEC_H
#define FD_VEC_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/* 0.5 LSB (ties up, saturated) unless stated; y may equal a same-type input */

/* ---- element-wise ---- */

void fd_add_q15(const fm_q15 *a, const fm_q15 *b, fm_q15 *y, uint32_t n);
void fd_add_q31(const fm_q31 *a, const fm_q31 *b, fm_q31 *y, uint32_t n);
void fd_sub_q15(const fm_q15 *a, const fm_q15 *b, fm_q15 *y, uint32_t n);
void fd_sub_q31(const fm_q31 *a, const fm_q31 *b, fm_q31 *y, uint32_t n);
void fd_mul_q15(const fm_q15 *a, const fm_q15 *b, fm_q15 *y, uint32_t n);
void fd_mul_q31(const fm_q31 *a, const fm_q31 *b, fm_q31 *y, uint32_t n);

/* x k 2^shift, shift -15..15 (q15) or -31..31 (q31) */
void fd_scale_q15(const fm_q15 *x, fm_q15 k, int32_t shift, fm_q15 *y,
                  uint32_t n);
void fd_scale_q31(const fm_q31 *x, fm_q31 k, int32_t shift, fm_q31 *y,
                  uint32_t n);

/* x 2^shift, shift -15..15 (q15) or -31..31 (q31) */
void fd_shift_q15(const fm_q15 *x, int32_t shift, fm_q15 *y, uint32_t n);
void fd_shift_q31(const fm_q31 *x, int32_t shift, fm_q31 *y, uint32_t n);

void fd_offset_q15(const fm_q15 *x, fm_q15 c, fm_q15 *y, uint32_t n);
void fd_offset_q31(const fm_q31 *x, fm_q31 c, fm_q31 *y, uint32_t n);
void fd_negate_q15(const fm_q15 *x, fm_q15 *y, uint32_t n);
void fd_negate_q31(const fm_q31 *x, fm_q31 *y, uint32_t n);
void fd_abs_q15(const fm_q15 *x, fm_q15 *y, uint32_t n);
void fd_abs_q31(const fm_q31 *x, fm_q31 *y, uint32_t n);
void fd_clip_q15(const fm_q15 *x, fm_q15 lo, fm_q15 hi, fm_q15 *y,
                 uint32_t n); /* lo <= hi */
void fd_clip_q31(const fm_q31 *x, fm_q31 lo, fm_q31 hi, fm_q31 *y, uint32_t n);
void fd_fill_q15(fm_q15 c, fm_q15 *y, uint32_t n);
void fd_fill_q31(fm_q31 c, fm_q31 *y, uint32_t n);
void fd_copy_q15(const fm_q15 *x, fm_q15 *y, uint32_t n);
void fd_copy_q31(const fm_q31 *x, fm_q31 *y, uint32_t n);

/* ---- conversions: as fm_fixed.h (float in: nearest even, NaN gives 0) ---- */

void fd_q15_to_q31(const fm_q15 *x, fm_q31 *y, uint32_t n);
void fd_q31_to_q15(const fm_q31 *x, fm_q15 *y, uint32_t n);
void fd_q7_to_q15(const int8_t *x, fm_q15 *y, uint32_t n);
void fd_q15_to_q7(const fm_q15 *x, int8_t *y, uint32_t n);
void fd_q15_to_f32(const fm_q15 *x, float *y, uint32_t n);
void fd_f32_to_q15(const float *x, fm_q15 *y, uint32_t n);
void fd_q31_to_f32(const fm_q31 *x, float *y, uint32_t n);
void fd_f32_to_q31(const float *x, fm_q31 *y, uint32_t n);

/* 20 log10 x in Q16.16, full scale 0 dB; x <= 0: INT32_MIN; 0.54 / 0.97 LSB */
void fd_db_q15(const fm_q15 *x, fm_q16 *y, uint32_t n);
void fd_db_q31(const fm_q31 *x, fm_q16 *y, uint32_t n);

/* ---- statistics ---- */

int64_t fd_sum_q15(const fm_q15 *x, uint32_t n); /* exact */
int64_t fd_sum_q31(const fm_q31 *x, uint32_t n); /* exact */
fm_q15 fd_mean_q15(const fm_q15 *x, uint32_t n); /* n >= 1 */
fm_q31 fd_mean_q31(const fm_q31 *x, uint32_t n);

/* q15: exact, Q30; q31: Q48, saturated beyond +-2^15 (CMSIS's formats) */
int64_t fd_dot_q15(const fm_q15 *a, const fm_q15 *b, uint32_t n);
int64_t fd_dot_q31(const fm_q31 *a, const fm_q31 *b, uint32_t n);
uint64_t fd_power_q15(const fm_q15 *x, uint32_t n);
uint64_t fd_power_q31(const fm_q31 *x, uint32_t n);

/* correctly rounded; rms n >= 1, var / std (over n - 1) 2 <= n < 2^24 */
fm_q15 fd_rms_q15(const fm_q15 *x, uint32_t n);
fm_q31 fd_rms_q31(const fm_q31 *x, uint32_t n);
fm_q15 fd_var_q15(const fm_q15 *x, uint32_t n);
fm_q31 fd_var_q31(const fm_q31 *x, uint32_t n);
fm_q15 fd_std_q15(const fm_q15 *x, uint32_t n);
fm_q31 fd_std_q31(const fm_q31 *x, uint32_t n);

/* n >= 1; index (may be NULL) of the first occurrence */
fm_q15 fd_min_q15(const fm_q15 *x, uint32_t n, uint32_t *index);
fm_q31 fd_min_q31(const fm_q31 *x, uint32_t n, uint32_t *index);
fm_q15 fd_max_q15(const fm_q15 *x, uint32_t n, uint32_t *index);
fm_q31 fd_max_q31(const fm_q31 *x, uint32_t n, uint32_t *index);
fm_q15 fd_absmax_q15(const fm_q15 *x, uint32_t n, uint32_t *index);
fm_q31 fd_absmax_q31(const fm_q31 *x, uint32_t n, uint32_t *index);
void fd_minmax_q15(const fm_q15 *x, uint32_t n, fm_q15 *min, fm_q15 *max);
void fd_minmax_q31(const fm_q31 *x, uint32_t n, fm_q31 *min, fm_q31 *max);

/* ---- complex vectors ---- */

void fd_cmul_q15(const fd_cq15 *a, const fd_cq15 *b, fd_cq15 *y, uint32_t n);
void fd_cmul_q31(const fd_cq31 *a, const fd_cq31 *b, fd_cq31 *y, uint32_t n);
void fd_cmul_real_q15(const fd_cq15 *a, const fm_q15 *b, fd_cq15 *y,
                      uint32_t n);
void fd_cmul_real_q31(const fd_cq31 *a, const fm_q31 *b, fd_cq31 *y,
                      uint32_t n);
void fd_conj_q15(const fd_cq15 *x, fd_cq15 *y, uint32_t n);
void fd_conj_q31(const fd_cq31 *x, fd_cq31 *y, uint32_t n);

/* |x| in UQ1.15 / UQ1.31: never saturates */
void fd_cmag_q15(const fd_cq15 *x, uint16_t *y, uint32_t n);
void fd_cmag_q31(const fd_cq31 *x, uint32_t *y, uint32_t n);

/* |x|^2: q15 exact in UQ2.30, q31 in UQ1.31 */
void fd_cmag2_q15(const fd_cq15 *x, uint32_t *y, uint32_t n);
void fd_cmag2_q31(const fd_cq31 *x, uint32_t *y, uint32_t n);

/* sum of a b (not conjugated), formats as fd_dot */
void fd_cdot_q15(const fd_cq15 *a, const fd_cq15 *b, uint32_t n, int64_t *re,
                 int64_t *im);
void fd_cdot_q31(const fd_cq31 *a, const fd_cq31 *b, uint32_t n, int64_t *re,
                 int64_t *im);

void fd_interleave_q15(const fm_q15 *re, const fm_q15 *im, fd_cq15 *y,
                       uint32_t n);
void fd_interleave_q31(const fm_q31 *re, const fm_q31 *im, fd_cq31 *y,
                       uint32_t n);
void fd_deinterleave_q15(const fd_cq15 *x, fm_q15 *re, fm_q15 *im, uint32_t n);
void fd_deinterleave_q31(const fd_cq31 *x, fm_q31 *re, fm_q31 *im, uint32_t n);

#endif /* FD_VEC_H */
