/**
 * @file fu_ring_probe.c
 * @brief Out-of-line copies of fu_ring.h's inline functions for asm-audit
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_ring.h"

bool probe_fu_ring_put(FU_Ring *r, uint8_t b);
bool probe_fu_ring_get(FU_Ring *r, uint8_t *b);

bool probe_fu_ring_put(FU_Ring *r, uint8_t b) { return fu_ring_put(r, b); }
bool probe_fu_ring_get(FU_Ring *r, uint8_t *b) { return fu_ring_get(r, b); }
