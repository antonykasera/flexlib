/**
 * @file fd_tone.h
 * @brief Goertzel single-bin detection, NCO and recursive oscillator
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FD_TONE_H
#define FD_TONE_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/* frequencies are phase steps per sample: a turn is 2^32 */

/* ---- Goertzel: one DFT term at any frequency ---- */

typedef struct {
  int32_t c;      /* 2 cos w, Q2.30 */
  int32_t cw, sw; /* cos w, sin w */
  int32_t s1, s2;
  uint32_t shift;
} FD_Goertzel;

/* inputs enter the int32 state as x 2^shift (q15) or x / 2^shift (q31); keep
 * n |x| / |sin w| under 2^31 in state units: past it the state wraps */
void fd_goertzel_init(FD_Goertzel *g, uint32_t freq, uint32_t shift);
void fd_goertzel_reset(FD_Goertzel *g);
void fd_goertzel_q15(FD_Goertzel *g, const fm_q15 *x, uint32_t n);
void fd_goertzel_q31(FD_Goertzel *g, const fm_q31 *x, uint32_t n);

/* sum of x[k] e^(jw (N - 1 - k)) over the N inputs so far, in state units */
void fd_goertzel_dft(const FD_Goertzel *g, int64_t *re, int64_t *im);

/* its squared magnitude (saturated) */
uint64_t fd_goertzel_power(const FD_Goertzel *g);

/* ---- NCO: sin and cos of a 32-bit phase, no drift ---- */

typedef struct {
  uint32_t phase, inc;
  int32_t dinc; /* added to inc every sample: a linear chirp */
} FD_Nco;

void fd_nco_init(FD_Nco *o, uint32_t phase, uint32_t inc, int32_t dinc);

/* s or c may be NULL; q31 as fm_sin_q31 (1.11 LSB), q15 0.5 LSB */
void fd_nco_q15(FD_Nco *o, fm_q15 *s, fm_q15 *c, uint32_t n);
void fd_nco_q31(FD_Nco *o, fm_q31 *s, fm_q31 *c, uint32_t n);

/* (cos, sin) pairs: e^(j phase) */
void fd_nco_cplx_q15(FD_Nco *o, fd_cq15 *y, uint32_t n);
void fd_nco_cplx_q31(FD_Nco *o, fd_cq31 *y, uint32_t n);

/* ---- recursive oscillator: a rotation per sample, renormalised ---- */

typedef struct {
  int32_t c, s;   /* cos, sin of the phase, Q30 */
  int32_t cw, sw; /* the rotation, q31 */
  uint32_t count;
} FD_Osc;

void fd_osc_init(FD_Osc *o, uint32_t phase, uint32_t inc);
void fd_osc_q15(FD_Osc *o, fm_q15 *s, fm_q15 *c, uint32_t n);
void fd_osc_q31(FD_Osc *o, fm_q31 *s, fm_q31 *c, uint32_t n);

#endif /* FD_TONE_H */
