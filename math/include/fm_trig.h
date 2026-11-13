/**
 * @file fm_trig.h
 * @brief Sine and cosine of binary angles in Q15 and Q31
 *
 * A full turn is 2^16 for the Q15 functions and 2^32 for Q31, so angle sums
 * wrap the way angles do. A result of 1 saturates to the largest value.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_TRIG_H
#define FM_TRIG_H

#include "fm_fixed.h"

/** $sin(angle)$, within 0.66 LSB. */
fm_q15 fm_sin_q15(uint16_t angle);
/** $cos(angle)$, within 0.66 LSB. */
fm_q15 fm_cos_q15(uint16_t angle);

/** fm_sin_q15() and fm_cos_q15() single call. */
FX_INLINE void fm_sincos_q15(uint16_t angle, fm_q15 *s, fm_q15 *c) {
  *s = fm_sin_q15(angle);
  *c = fm_cos_q15(angle);
}

/** $sin(angle)$, within 1.11 LSB. */
fm_q31 fm_sin_q31(uint32_t angle);
/** $cos(angle)$, within 1.11 LSB. */
fm_q31 fm_cos_q31(uint32_t angle);
/** fm_sin_q31() and fm_cos_q31() single call, the same bits. */
void fm_sincos_q31(uint32_t angle, fm_q31 *s, fm_q31 *c);

#endif /* FM_TRIG_H */
