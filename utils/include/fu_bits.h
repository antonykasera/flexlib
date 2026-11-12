/**
 * @file fu_bits.h
 * @brief Bit counts, bit and byte reversal, endian loads/stores, sign extension
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FU_BITS_H
#define FU_BITS_H

#include "fx_arch.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ---- counts: 0 in gives the full width (32 or 64) ---- */

FX_INLINE uint32_t fu_clz(uint32_t x) { return fx_clz(x); }
FX_INLINE uint32_t fu_ctz(uint32_t x) { return fx_ctz(x); }

FX_INLINE uint32_t fu_clz64(uint64_t x) {
  uint32_t hi = (uint32_t)(x >> 32);
  return hi != 0U ? fx_clz(hi) : 32U + fx_clz((uint32_t)x);
}

FX_INLINE uint32_t fu_ctz64(uint64_t x) {
  uint32_t lo = (uint32_t)x;
  return lo != 0U ? fx_ctz(lo) : 32U + fx_ctz((uint32_t)(x >> 32));
}

FX_INLINE uint32_t fu_popcount(uint32_t x) {
  x = x - ((x >> 1) & 0x55555555U);
  x = (x & 0x33333333U) + ((x >> 2) & 0x33333333U);
  x = (x + (x >> 4)) & 0x0F0F0F0FU;
  return (x * 0x01010101U) >> 24;
}

FX_INLINE bool fu_is_pow2(uint32_t x) { return x != 0U && (x & (x - 1U)) == 0U; }

/* ---- reversal ---- */

FX_INLINE uint32_t fu_rbit(uint32_t x) { return fx_rbit(x); }
FX_INLINE uint32_t fu_bswap32(uint32_t x) { return fx_rev(x); }
FX_INLINE uint16_t fu_bswap16(uint16_t x) { return __builtin_bswap16(x); }

/* ---- loads and stores of any alignment ---- */

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define FU_LE16(x) fu_bswap16(x)
#define FU_LE32(x) fu_bswap32(x)
#define FU_BE16(x) (x)
#define FU_BE32(x) (x)
#else
#define FU_LE16(x) (x)
#define FU_LE32(x) (x)
#define FU_BE16(x) fu_bswap16(x)
#define FU_BE32(x) fu_bswap32(x)
#endif

FX_INLINE uint16_t fu_load_le16(const void *p) {
  uint16_t v;
  memcpy(&v, p, sizeof v);
  return FU_LE16(v);
}

FX_INLINE uint32_t fu_load_le32(const void *p) {
  uint32_t v;
  memcpy(&v, p, sizeof v);
  return FU_LE32(v);
}

FX_INLINE uint16_t fu_load_be16(const void *p) {
  uint16_t v;
  memcpy(&v, p, sizeof v);
  return FU_BE16(v);
}

FX_INLINE uint32_t fu_load_be32(const void *p) {
  uint32_t v;
  memcpy(&v, p, sizeof v);
  return FU_BE32(v);
}

FX_INLINE void fu_store_le16(void *p, uint16_t v) {
  v = FU_LE16(v);
  memcpy(p, &v, sizeof v);
}

FX_INLINE void fu_store_le32(void *p, uint32_t v) {
  v = FU_LE32(v);
  memcpy(p, &v, sizeof v);
}

FX_INLINE void fu_store_be16(void *p, uint16_t v) {
  v = FU_BE16(v);
  memcpy(p, &v, sizeof v);
}

FX_INLINE void fu_store_be32(void *p, uint32_t v) {
  v = FU_BE32(v);
  memcpy(p, &v, sizeof v);
}

/* ---- sign extension ---- */

/* low `bits` of x as signed, bits in 1..32 */
FX_INLINE int32_t fu_sext(uint32_t x, uint32_t bits) {
  FX_ASSERT(bits >= 1U && bits <= 32U);
  uint32_t s = 32U - bits;
  return (int32_t)(x << s) >> s;
}

#endif /* FU_BITS_H */
