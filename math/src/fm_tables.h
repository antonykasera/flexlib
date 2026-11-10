/**
 * @file fm_tables.h
 * @brief Lookup tables for flexmath (generated into fm_tables.c)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_TABLES_H
#define FM_TABLES_H

#include "fx_arch.h"
#include <stdint.h>

/* FM_LUT_IN_RAM: tables go to .data (2.8 KB RAM), no flash wait states */
#ifdef FM_LUT_IN_RAM
#define FM_LUT FX_ALIGN_LINE __attribute__((section(".data.fm_lut")))
#else
#define FM_LUT FX_ALIGN_LINE
#endif

/* seeds for x in [2^30, 2^32) by (x >> 25) - 32: sqrt(x) >> 16, 1/sqrt, Q15 */
extern const uint16_t fm_sqrt_seed[96];
extern const uint16_t fm_rsqrt_seed[96];

/* sin(k pi / 512) in Q31, k = 0..256 (2^31 at k = 256), plus a pad entry */
extern const uint32_t fm_sin_tab[258];

/* atan(k / 128) in Q34 turns, k = 0..128 (2^31 at k = 128), plus a pad entry */
extern const uint32_t fm_atan_tab[130];

/* bucket k = top 6 bits of the mantissa f; f r_k is within 2^-7 of 1 */
extern const uint32_t fm_log2_rcp[64];
extern const uint32_t fm_log2_tab[64];
/* the 8 bits below fm_log2_tab (Q40 together), signed */
extern const int8_t fm_log2_lo[64];

/* 2^(k / 64) - 1 in Q38: the top 32 bits, then the low 6 */
extern const uint32_t fm_exp2_tab[64];
extern const uint8_t fm_exp2_lo[64];

/* 1 / (2 pi) in 32-bit words after one zero word */
extern const uint32_t fm_inv2pi_tab[8];

/* |coefficients| in Q31, constant first, signs alternating from + */
extern const uint32_t fm_sinf_poly[4];
extern const uint32_t fm_cosf_poly[4];

/* the same in u = r / (pi/4), Q32 */
extern const uint32_t fm_sinf_upoly[5];
extern const uint32_t fm_cosf_upoly[4];

/* atan s = s (1 - z T(z)): |T| coefficients in Q31, signs alternating */
extern const uint32_t fm_atanf_poly[5];

/* log2(1 + u) = u P(u): P - 1, then |u, u^2, u^3 coefficients|, Q31 */
extern const uint32_t fm_log2f_poly[4];

#endif /* FM_TABLES_H */
