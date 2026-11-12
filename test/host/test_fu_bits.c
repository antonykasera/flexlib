/**
 * @file test_fu_bits.c
 * @brief Host tests for fu_bits.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fu_bits.h"
#include "test_stats.h"

static uint32_t ref_popcount(uint64_t x) {
  uint32_t n = 0U;
  for (; x != 0U; x >>= 1) {
    n += (uint32_t)(x & 1U);
  }
  return n;
}

static uint32_t ref_clz64(uint64_t x) {
  uint32_t n = 0U;
  for (uint64_t m = 1ULL << 63; m != 0U && (x & m) == 0U; m >>= 1) {
    n++;
  }
  return n;
}

static uint32_t ref_ctz64(uint64_t x) {
  uint32_t n = 0U;
  for (uint64_t m = 1U; m != 0U && (x & m) == 0U; m <<= 1) {
    n++;
  }
  return n;
}

/* the low `bits` of x, read as two's complement */
static int64_t ref_sext(uint32_t x, uint32_t bits) {
  uint64_t v = x & (uint64_t)((1ULL << bits) - 1U);
  return (v >> (bits - 1U)) != 0U ? (int64_t)v - (int64_t)(1ULL << bits)
                                  : (int64_t)v;
}

void test_fu_counts_edges(void) {
  ASSERT_EQ(fu_clz(0U), 32);
  ASSERT_EQ(fu_ctz(0U), 32);
  ASSERT_EQ(fu_clz64(0U), 64);
  ASSERT_EQ(fu_ctz64(0U), 64);
  ASSERT_EQ(fu_popcount(0U), 0);
  ASSERT_EQ(fu_popcount(0xFFFFFFFFU), 32);
  for (uint32_t k = 0U; k < 64U; k++) {
    uint64_t p = 1ULL << k;
    ASSERT_EQ(fu_clz64(p), 63U - k);
    ASSERT_EQ(fu_ctz64(p), k);
    ASSERT_EQ(fu_clz64(p | (p - 1U)), 63U - k);
    ASSERT_EQ(fu_ctz64(p | (1ULL << 63)), k);
  }
}

void test_fu_counts_random(void) {
  for (uint32_t x = 0U; x < 65536U; x++) {
    ASSERT_EQ(fu_popcount(x), ref_popcount(x));
    ASSERT_EQ(fu_popcount(x << 16), ref_popcount(x));
  }
  uint64_t s = 3U;
  for (int i = 0; i < 1000000; i++) {
    uint64_t x = test_rng_next(&s) >> (test_rng_u32(&s) & 63U);
    ASSERT_EQ(fu_popcount((uint32_t)x), ref_popcount((uint32_t)x));
    ASSERT_EQ(fu_clz64(x), ref_clz64(x));
    ASSERT_EQ(fu_ctz64(x), ref_ctz64(x));
  }
}

void test_fu_is_pow2(void) {
  ASSERT_TRUE(!fu_is_pow2(0U));
  ASSERT_TRUE(!fu_is_pow2(3U));
  ASSERT_TRUE(!fu_is_pow2(0xFFFFFFFFU));
  for (uint32_t k = 0U; k < 32U; k++) {
    ASSERT_TRUE(fu_is_pow2(1U << k));
    ASSERT_TRUE(k < 2U || !fu_is_pow2((1U << k) + 1U));
  }
}

void test_fu_reversal(void) {
  for (uint32_t x = 0U; x < 65536U; x++) {
    uint16_t v = (uint16_t)x;
    ASSERT_EQ(fu_bswap16(v), (uint16_t)((v >> 8) | (v << 8)));
  }
  ASSERT_EQ(fu_bswap32(0x12345678U), 0x78563412U);
  ASSERT_EQ(fu_rbit(0x00000001U), 0x80000000U);
}

void test_fu_load_store(void) {
  uint8_t buf[12];
  for (uint8_t i = 0U; i < sizeof buf; i++) {
    buf[i] = (uint8_t)(0x10U + i);
  }
  for (unsigned off = 0U; off < 8U; off++) { /* every alignment */
    const uint8_t *p = &buf[off];
    uint32_t le = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
                  (uint32_t)p[3] << 24;
    uint32_t be = (uint32_t)p[3] | (uint32_t)p[2] << 8 | (uint32_t)p[1] << 16 |
                  (uint32_t)p[0] << 24;
    ASSERT_EQ(fu_load_le32(p), le);
    ASSERT_EQ(fu_load_be32(p), be);
    ASSERT_EQ(fu_load_le16(p), (uint16_t)(p[0] | p[1] << 8));
    ASSERT_EQ(fu_load_be16(p), (uint16_t)(p[1] | p[0] << 8));
  }

  for (unsigned off = 0U; off < 8U; off++) {
    uint8_t out[12] = {0};
    fu_store_be32(&out[off], 0xA1B2C3D4U);
    ASSERT_EQ(out[off], 0xA1);
    ASSERT_EQ(out[off + 3U], 0xD4);
    ASSERT_EQ(fu_load_be32(&out[off]), 0xA1B2C3D4U);
    fu_store_le32(&out[off], 0xA1B2C3D4U);
    ASSERT_EQ(out[off], 0xD4);
    ASSERT_EQ(fu_load_le32(&out[off]), 0xA1B2C3D4U);
    fu_store_be16(&out[off], 0xBEEFU);
    ASSERT_EQ(out[off], 0xBE);
    fu_store_le16(&out[off], 0xBEEFU);
    ASSERT_EQ(out[off], 0xEF);
    ASSERT_EQ(fu_load_le16(&out[off]), 0xBEEF);
    /* nothing written outside the 4 bytes */
    for (unsigned i = 0U; i < sizeof out; i++) {
      if (i < off || i >= off + 4U) {
        ASSERT_EQ(out[i], 0);
      }
    }
  }
}

void test_fu_sext(void) {
  for (uint32_t x = 0U; x < 4096U; x++) { /* every 12-bit value */
    ASSERT_EQ(fu_sext(x, 12U), ref_sext(x, 12U));
  }
  ASSERT_EQ(fu_sext(0x800U, 12U), -2048);
  ASSERT_EQ(fu_sext(0x7FFU, 12U), 2047);
  ASSERT_EQ(fu_sext(1U, 1U), -1);
  ASSERT_EQ(fu_sext(0x80000000U, 32U), INT32_MIN);

  uint64_t s = 4U;
  for (int i = 0; i < 100000; i++) {
    uint32_t x = test_rng_u32(&s);
    uint32_t bits = 1U + (test_rng_u32(&s) & 31U);
    ASSERT_EQ(fu_sext(x, bits), ref_sext(x, bits));
  }
}
