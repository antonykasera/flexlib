/**
 * @file fx_arch_probe.c
 * @brief Out-of-line copies of the fx_arch.h wrappers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fx_arch.h"

uint32_t probe_clz(uint32_t x);
uint32_t probe_ctz(uint32_t x);
uint32_t probe_rbit(uint32_t x);
uint32_t probe_rev(uint32_t x);
uint32_t probe_rev16(uint32_t x);
int32_t probe_ssat16(int32_t x);
uint32_t probe_usat8(int32_t x);
int32_t probe_smull_hi(int32_t a, int32_t b);
uint32_t probe_umull_hi(uint32_t a, uint32_t b);
int64_t probe_smlal(int64_t acc, int32_t a, int32_t b);
uint64_t probe_umlal(uint64_t acc, uint32_t a, uint32_t b);
void probe_dmb(void);

uint32_t probe_clz(uint32_t x) { return fx_clz(x); }
uint32_t probe_ctz(uint32_t x) { return fx_ctz(x); }
uint32_t probe_rbit(uint32_t x) { return fx_rbit(x); }
uint32_t probe_rev(uint32_t x) { return fx_rev(x); }
uint32_t probe_rev16(uint32_t x) { return fx_rev16(x); }
int32_t probe_ssat16(int32_t x) { return FX_SSAT(x, 16); }
uint32_t probe_usat8(int32_t x) { return FX_USAT(x, 8); }
int32_t probe_smull_hi(int32_t a, int32_t b) { return fx_smull_hi(a, b); }
uint32_t probe_umull_hi(uint32_t a, uint32_t b) { return fx_umull_hi(a, b); }
int64_t probe_smlal(int64_t acc, int32_t a, int32_t b) {
  return fx_smlal(acc, a, b);
}
uint64_t probe_umlal(uint64_t acc, uint32_t a, uint32_t b) {
  return fx_umlal(acc, a, b);
}
void probe_dmb(void) { fx_dmb(); }
