/**
 * @file test_fx_arch.c
 * @brief Host tests for the plain-C paths of fx_arch.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fx_arch.h"
#include "test_stats.h"

static uint32_t ref_rbit(uint32_t x) {
  uint32_t r = 0U;
  for (unsigned i = 0U; i < 32U; i++) {
    r |= ((x >> i) & 1U) << (31U - i);
  }
  return r;
}

static int64_t ref_clamp(int64_t x, int64_t lo, int64_t hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

/* floor(p / 2^32) without relying on >> of a negative value */
static int64_t ref_floor_div32(int64_t p) {
  const int64_t d = 4294967296LL;
  return p >= 0 ? p / d : -((-p + d - 1) / d);
}

void test_fx_clz_ctz(void) {
  ASSERT_EQ(fx_clz(0U), 32);
  ASSERT_EQ(fx_ctz(0U), 32);
  ASSERT_EQ(fx_clz(0xFFFFFFFFU), 0);
  ASSERT_EQ(fx_ctz(0xFFFFFFFFU), 0);
  for (uint32_t k = 0U; k < 32U; k++) {
    uint32_t p = 1U << k;
    ASSERT_EQ(fx_clz(p), 31U - k);
    ASSERT_EQ(fx_clz(p | (p - 1U)), 31U - k);
    ASSERT_EQ(fx_ctz(p), k);
    ASSERT_EQ(fx_ctz(p | 0x80000000U), k);
  }
}

void test_fx_rbit_rev(void) {
  ASSERT_EQ(fx_rbit(1U), 0x80000000U);
  ASSERT_EQ(fx_rbit(0x12345678U), 0x1E6A2C48U);
  ASSERT_EQ(fx_rev(0x12345678U), 0x78563412U);
  ASSERT_EQ(fx_rev16(0x12345678U), 0x34127856U);
  ASSERT_EQ(fx_rotl32(0x80000001U, 1U), 0x00000003U);
  ASSERT_EQ(fx_rotl32(0x12345678U, 0U), 0x12345678U);
  ASSERT_EQ(fx_rotl32(0x12345678U, 8U), 0x34567812U);

  uint64_t s = 1U;
  for (int i = 0; i < 100000; i++) {
    uint32_t x = test_rng_u32(&s);
    ASSERT_EQ(fx_rbit(x), ref_rbit(x));
    ASSERT_EQ(fx_rev(fx_rev(x)), x);
    ASSERT_EQ(fx_rev16(fx_rev16(x)), x);
  }
}

void test_fx_ssat(void) {
  for (int32_t x = -70000; x <= 70000; x++) {
    ASSERT_EQ(FX_SSAT(x, 16), ref_clamp(x, -32768, 32767));
  }
  ASSERT_EQ(FX_SSAT(5, 1), 0);
  ASSERT_EQ(FX_SSAT(-5, 1), -1);
  ASSERT_EQ(FX_SSAT(INT32_MIN, 31), -1073741824);
  ASSERT_EQ(FX_SSAT(INT32_MAX, 31), 1073741823);
  ASSERT_EQ(FX_SSAT(INT32_MIN, 32), INT32_MIN);
  ASSERT_EQ(FX_SSAT(INT32_MAX, 32), INT32_MAX);
}

void test_fx_usat(void) {
  for (int32_t x = -1000; x <= 1000; x++) {
    ASSERT_EQ(FX_USAT(x, 8), ref_clamp(x, 0, 255));
  }
  ASSERT_EQ(FX_USAT(12345, 0), 0);
  ASSERT_EQ(FX_USAT(INT32_MIN, 31), 0);
  ASSERT_EQ(FX_USAT(INT32_MAX, 31), INT32_MAX);
}

void test_fx_multiplies(void) {
  ASSERT_EQ(fx_smull_hi(INT32_MIN, INT32_MIN), 1073741824);
  ASSERT_EQ(fx_smull_hi(INT32_MIN, INT32_MAX), -1073741824);
  ASSERT_EQ(fx_smull_hi(-1, 1), -1); /* floor, not truncation */
  ASSERT_EQ(fx_umull_hi(0xFFFFFFFFU, 0xFFFFFFFFU), 0xFFFFFFFEU);

  uint64_t s = 2U;
  for (int i = 0; i < 100000; i++) {
    int32_t a = (int32_t)test_rng_u32(&s);
    int32_t b = (int32_t)test_rng_u32(&s);
    int64_t p = (int64_t)a * (int64_t)b;
    ASSERT_EQ(fx_smull_hi(a, b), ref_floor_div32(p));
    ASSERT_EQ(fx_smlal(-12345, a, b), p - 12345);

    uint32_t ua = (uint32_t)a;
    uint32_t ub = (uint32_t)b;
    uint64_t up = (uint64_t)ua * (uint64_t)ub;
    ASSERT_EQ(fx_umull_hi(ua, ub), up >> 32);
    ASSERT_EQ(fx_umlal(7U, ua, ub) - up, 7U);
  }
}
