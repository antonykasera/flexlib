/**
 * @file fu_bits_probe.c
 * @brief Out-of-line copies of fu_bits.h for asm-audit
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fu_bits.h"

uint32_t probe_fu_clz64(uint64_t x);
uint32_t probe_fu_ctz64(uint64_t x);
uint32_t probe_fu_popcount(uint32_t x);
bool probe_fu_is_pow2(uint32_t x);
uint16_t probe_fu_bswap16(uint16_t x);
uint32_t probe_fu_load_le32(const void *p);
uint32_t probe_fu_load_be32(const void *p);
uint16_t probe_fu_load_be16(const void *p);
void probe_fu_store_be32(void *p, uint32_t v);
void probe_fu_store_le16(void *p, uint16_t v);
int32_t probe_fu_sext12(uint32_t x);
int32_t probe_fu_sext(uint32_t x, uint32_t bits);

uint32_t probe_fu_clz64(uint64_t x) { return fu_clz64(x); }
uint32_t probe_fu_ctz64(uint64_t x) { return fu_ctz64(x); }
uint32_t probe_fu_popcount(uint32_t x) { return fu_popcount(x); }
bool probe_fu_is_pow2(uint32_t x) { return fu_is_pow2(x); }
uint16_t probe_fu_bswap16(uint16_t x) { return fu_bswap16(x); }
uint32_t probe_fu_load_le32(const void *p) { return fu_load_le32(p); }
uint32_t probe_fu_load_be32(const void *p) { return fu_load_be32(p); }
uint16_t probe_fu_load_be16(const void *p) { return fu_load_be16(p); }
void probe_fu_store_be32(void *p, uint32_t v) { fu_store_be32(p, v); }
void probe_fu_store_le16(void *p, uint16_t v) { fu_store_le16(p, v); }
int32_t probe_fu_sext12(uint32_t x) { return fu_sext(x, 12U); }
int32_t probe_fu_sext(uint32_t x, uint32_t bits) { return fu_sext(x, bits); }
