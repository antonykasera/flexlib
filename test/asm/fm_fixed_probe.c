/**
 * @file fm_fixed_probe.c
 * @brief Out-of-line copies of fm_fixed.h's inline functions for asm-audit
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_fixed.h"

fm_q15 probe_fm_add_q15(fm_q15 a, fm_q15 b);
int32_t probe_fm_add_q31(int32_t a, int32_t b);
int32_t probe_fm_sub_q31(int32_t a, int32_t b);
fm_q15 probe_fm_mul_q15(fm_q15 a, fm_q15 b);
fm_q31 probe_fm_mul_q31(fm_q31 a, fm_q31 b);
fm_q16 probe_fm_mul_q16(fm_q16 a, fm_q16 b);
int64_t probe_fm_mac_q31(int64_t acc, fm_q31 a, fm_q31 b);

fm_q15 probe_fm_add_q15(fm_q15 a, fm_q15 b) { return fm_add_q15(a, b); }
int32_t probe_fm_add_q31(int32_t a, int32_t b) { return fm_add_q31(a, b); }
int32_t probe_fm_sub_q31(int32_t a, int32_t b) { return fm_sub_q31(a, b); }
fm_q15 probe_fm_mul_q15(fm_q15 a, fm_q15 b) { return fm_mul_q15(a, b); }
fm_q31 probe_fm_mul_q31(fm_q31 a, fm_q31 b) { return fm_mul_q31(a, b); }
fm_q16 probe_fm_mul_q16(fm_q16 a, fm_q16 b) { return fm_mul_q16(a, b); }
int64_t probe_fm_mac_q31(int64_t acc, fm_q31 a, fm_q31 b) {
  return fm_mac_q31(acc, a, b);
}
