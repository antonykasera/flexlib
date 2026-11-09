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

/* seeds for x in [2^30, 2^32), indexed by (x >> 25) - 32: sqrt(x) >> 16 and
 * 1/sqrt(x / 2^32) / 2 in Q15, at each bucket's midpoint */
extern const uint16_t fm_sqrt_seed[96];
extern const uint16_t fm_rsqrt_seed[96];

#endif /* FM_TABLES_H */
