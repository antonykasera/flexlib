/**
 * @file fu_rand_probe.c
 * @brief Out-of-line copies of fu_rand.h's inline generators for asm-audit
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fu_rand.h"

uint32_t probe_fu_xorshift32(FU_Xorshift32 *r);
uint32_t probe_fu_xoshiro128ss(FU_Xoshiro128 *r);
uint32_t probe_fx_rotl32(uint32_t x, uint32_t k);
uint32_t probe_fu_xoshiro128_below(FU_Xoshiro128 *r, uint32_t n);

uint32_t probe_fu_xorshift32(FU_Xorshift32 *r) { return fu_xorshift32(r); }
uint32_t probe_fu_xoshiro128ss(FU_Xoshiro128 *r) { return fu_xoshiro128ss(r); }
uint32_t probe_fx_rotl32(uint32_t x, uint32_t k) { return fx_rotl32(x, k); }
uint32_t probe_fu_xoshiro128_below(FU_Xoshiro128 *r, uint32_t n) {
  return fu_xoshiro128_below(r, n);
}
