/**
 * @file fu_debounce_probe.c
 * @brief Out-of-line copies of fu_debounce.h's inline functions for asm-audit
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fu_debounce.h"

bool probe_fu_debounce(FU_Debounce *d, bool raw);
uint32_t probe_fu_debounce32(FU_Debounce32 *d, uint32_t raw);

bool probe_fu_debounce(FU_Debounce *d, bool raw) {
  return fu_debounce(d, raw);
}
uint32_t probe_fu_debounce32(FU_Debounce32 *d, uint32_t raw) {
  return fu_debounce32(d, raw);
}
