/**
 * @file fm_trig.h
 * @brief Sine and cosine of binary angles in Q15 and Q31
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * A full turn is 2^16 (q15) or 2^32 (q31), so angles wrap for free; CMSIS-DSP's
 * q15 and q31 angles are ours shifted right by one. +1 saturates to the largest
 * value, -1 is exact. sincos gives exactly what sin and cos give.
 */

#ifndef FM_TRIG_H
#define FM_TRIG_H

#include "fm_fixed.h"

/* within 0.66 LSB (0.650 on all 65536 angles) */
fm_q15 fm_sin_q15(uint16_t angle);
fm_q15 fm_cos_q15(uint16_t angle);

/* the two share no table work, so two calls are fastest */
FX_INLINE void fm_sincos_q15(uint16_t angle, fm_q15 *s, fm_q15 *c) {
  *s = fm_sin_q15(angle);
  *c = fm_cos_q15(angle);
}

/* within 1.11 LSB (1.105 on all 2^32 angles) */
fm_q31 fm_sin_q31(uint32_t angle);
fm_q31 fm_cos_q31(uint32_t angle);
void fm_sincos_q31(uint32_t angle, fm_q31 *s, fm_q31 *c);

#endif /* FM_TRIG_H */
