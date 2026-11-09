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

/* FM_LUT_IN_RAM: the tables go to .data, copied to SRAM at startup: no flash
 * wait states or cache misses, for their size in RAM (2.7 KB) */
#ifdef FM_LUT_IN_RAM
#define FM_LUT FX_ALIGN_LINE __attribute__((section(".data.fm_lut")))
#else
#define FM_LUT FX_ALIGN_LINE
#endif

/* seeds for x in [2^30, 2^32), indexed by (x >> 25) - 32: sqrt(x) >> 16 and
 * 1/sqrt(x / 2^32) / 2 in Q15, at each bucket's midpoint */
extern const uint16_t fm_sqrt_seed[96];
extern const uint16_t fm_rsqrt_seed[96];

/* sin(k pi / 512) in Q31, k = 0..256 (2^31 at k = 256), plus a pad entry */
extern const uint32_t fm_sin_tab[258];

/* atan(k / 128) in Q34 turns, k = 0..128 (2^31 at k = 128), plus a pad entry */
extern const uint32_t fm_atan_tab[130];

/* log2 of the mantissa f in [1, 2), bucket k = top 6 bits: f r_k is within
 * 2^-7 of 1, with r_k = fm_log2_rcp[k] / 2^32 and -log2 r_k in fm_log2_tab */
extern const uint32_t fm_log2_rcp[64];
extern const uint32_t fm_log2_tab[64];

/* 2^(k / 64) - 1 in Q38: the top 32 bits, then the low 6 */
extern const uint32_t fm_exp2_tab[64];
extern const uint8_t fm_exp2_lo[64];

#endif /* FM_TABLES_H */
