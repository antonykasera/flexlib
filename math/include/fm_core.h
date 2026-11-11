/**
 * @file fm_core.h
 * @brief flexmath tables and cores shared with flexdsp (internal, not API)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_CORE_H
#define FM_CORE_H

#include <stdint.h>

/* sin(k pi / 512) in Q31, k = 0..256 (2^31 at k = 256), plus a pad entry */
extern const uint32_t fm_sin_tab[258];

/* log2 of x / 2^16 in Q32, x > 0, before fm_log2_q16's rounding */
int64_t fm_log2_core(uint32_t x);

#endif /* FM_CORE_H */
