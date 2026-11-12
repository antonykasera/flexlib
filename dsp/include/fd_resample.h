/**
 * @file fd_resample.h
 * @brief Sample-rate conversion: rational l / m resampling, half-band by 2,
 * CIC decimation
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_RESAMPLE_H
#define FD_RESAMPLE_H

#include "fd_fir.h"
#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/* ---- rational l / m, 0.5 LSB: h at l times the rate with gain l; the same
 * bits as fd_fir_interp by l, then every m-th output ---- */

#define FD_RESAMPLE_STATE_LEN(ntaps, l, chunk)                                 \
  FD_FIR_INTERP_STATE_LEN(ntaps, l, chunk)
/* the most outputs n inputs give */
#define FD_RESAMPLE_OUT_MAX(n, l, m) (((n) * (l) + (m) - 1U) / (m))

typedef struct {
  FD_FirInterpQ15 ip;
  uint32_t m;
  uint32_t ph; /* next output at the l-times rate, from the next input */
} FD_ResampleQ15;

typedef struct {
  FD_FirInterpQ31 ip;
  uint32_t m;
  uint32_t ph;
} FD_ResampleQ31;

/* l, m >= 1; l state_len + m < 2^32 */
void fd_resample_init_q15(FD_ResampleQ15 *r, const fm_q15 *h, uint32_t ntaps,
                          uint32_t l, uint32_t m, fm_q15 *state,
                          uint32_t state_len);
void fd_resample_init_q31(FD_ResampleQ31 *r, const fm_q31 *h, uint32_t ntaps,
                          uint32_t l, uint32_t m, fm_q31 *state,
                          uint32_t state_len);
void fd_resample_reset_q15(FD_ResampleQ15 *r);
void fd_resample_reset_q31(FD_ResampleQ31 *r);
/* any n; returns the outputs written; y must not overlap x */
uint32_t fd_resample_q15(FD_ResampleQ15 *r, const fm_q15 *x, fm_q15 *y,
                         uint32_t n);
uint32_t fd_resample_q31(FD_ResampleQ31 *r, const fm_q31 *x, fm_q31 *y,
                         uint32_t n);

/* lowpass at fc (0, 1) of the lower Nyquist, gain l (not timed) */
FD_Status fd_resample_design(float *h, uint32_t ntaps, uint32_t l, uint32_t m,
                             float fc, FD_Window win);

/* ---- half-band by 2, 0.5 LSB: hb holds the 2k taps at odd offsets from the
 * centre (gain 1, centre 1/2 implied), hb[j] for the sample 2j back ---- */

/* decimate: the full filter's 2nd, 4th, ... outputs, as fd_fir_decim by 2 */
#define FD_HALFBAND_DECIM_STATE_LEN(k, chunk) (4U * (k) - 2U + (chunk))
/* interpolate: y[2i] = 2 sum hb[j] x[i - j], y[2i + 1] = x[i - k + 1] */
#define FD_HALFBAND_INTERP_STATE_LEN(k, chunk) (2U * (k) - 1U + (chunk))

typedef struct {
  FD_FirQ15 fir; /* over the 4k - 1 (interpolator: 2k) sample window */
  uint32_t k;
  uint32_t phase;
} FD_HalfbandQ15;

typedef struct {
  FD_FirQ31 fir;
  uint32_t k;
  uint32_t phase;
} FD_HalfbandQ31;

/* k >= 1 */
void fd_halfband_decim_init_q15(FD_HalfbandQ15 *d, const fm_q15 *hb, uint32_t k,
                                fm_q15 *state, uint32_t state_len);
void fd_halfband_decim_init_q31(FD_HalfbandQ31 *d, const fm_q31 *hb, uint32_t k,
                                fm_q31 *state, uint32_t state_len);
void fd_halfband_interp_init_q15(FD_HalfbandQ15 *p, const fm_q15 *hb,
                                 uint32_t k, fm_q15 *state, uint32_t state_len);
void fd_halfband_interp_init_q31(FD_HalfbandQ31 *p, const fm_q31 *hb,
                                 uint32_t k, fm_q31 *state, uint32_t state_len);
void fd_halfband_reset_q15(FD_HalfbandQ15 *f);
void fd_halfband_reset_q31(FD_HalfbandQ31 *f);
/* returns the outputs written: n / 2 when n is even */
uint32_t fd_halfband_decim_q15(FD_HalfbandQ15 *d, const fm_q15 *x, fm_q15 *y,
                               uint32_t n);
uint32_t fd_halfband_decim_q31(FD_HalfbandQ31 *d, const fm_q31 *x, fm_q31 *y,
                               uint32_t n);
/* writes 2 n outputs; y must not overlap x */
void fd_halfband_interp_q15(FD_HalfbandQ15 *p, const fm_q15 *x, fm_q15 *y,
                            uint32_t n);
void fd_halfband_interp_q31(FD_HalfbandQ31 *p, const fm_q31 *x, fm_q31 *y,
                            uint32_t n);

/* 2k taps summing to 1/2: gain 1 at DC, 0 at Nyquist (not timed) */
FD_Status fd_halfband_design(float *hb, uint32_t k, FD_Window win);

/* ---- CIC decimator: exact in 32- or 64-bit registers; q31 out = gain x /
 * 2^(in_bits - 1), rounded once (0.5 LSB) only past 32 bits ---- */

#define FD_CIC_MAX_ORDER 6U

typedef struct {
  uint32_t order, ratio, count;
  uint32_t wide;  /* 64-bit registers */
  uint32_t shift; /* 32 - bits (left), or bits - 32 (right, wide) */
  uint32_t growth;
  uint64_t integ[FD_CIC_MAX_ORDER], comb[FD_CIC_MAX_ORDER];
} FD_Cic;

/* order 1 .. 6, ratio >= 1, x within in_bits (1 .. 32); else FD_ERR_RANGE */
FD_Status fd_cic_init(FD_Cic *c, uint32_t order, uint32_t ratio,
                      uint32_t in_bits);
void fd_cic_reset(FD_Cic *c);
/* returns the outputs written: n / ratio from a fresh start */
uint32_t fd_cic_decim_q15(FD_Cic *c, const fm_q15 *x, fm_q31 *y, uint32_t n);
uint32_t fd_cic_decim_q31(FD_Cic *c, const fm_q31 *x, fm_q31 *y, uint32_t n);
/* ratio^order / 2^growth in (0.5, 1], Q2.30 (2^30 = 1) */
int32_t fd_cic_gain(const FD_Cic *c);

/* FIR at the output rate: 1 / |CIC| up to fc (0, 0.5), 0 above (not timed) */
FD_Status fd_cic_comp_design(float *h, uint32_t ntaps, uint32_t order,
                             uint32_t ratio, float fc, FD_Window win);

#endif /* FD_RESAMPLE_H */
