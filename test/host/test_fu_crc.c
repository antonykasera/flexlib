/**
 * @file test_fu_crc.c
 * @brief Host tests for fu_crc.h, against a bitwise reference
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_crc.h"
#include "test_stats.h"
#include <stdbool.h>

typedef struct {
  uint32_t width, poly, init, xorout; /* poly as written MSB first */
  bool refl;
} CrcSpec;

static const CrcSpec SMBUS = {8U, 0x07U, 0x00U, 0x00U, false};
static const CrcSpec MAXIM = {8U, 0x31U, 0x00U, 0x00U, true};
static const CrcSpec CCITT = {16U, 0x1021U, 0xFFFFU, 0x0000U, false};
static const CrcSpec MODBUS = {16U, 0x8005U, 0xFFFFU, 0x0000U, true};
static const CrcSpec CRC32 = {32U, 0x04C11DB7U, 0xFFFFFFFFU, 0xFFFFFFFFU, true};

static uint32_t reflect(uint32_t v, uint32_t bits) {
  uint32_t r = 0U;
  for (uint32_t i = 0U; i < bits; i++) {
    r |= ((v >> i) & 1U) << (bits - 1U - i);
  }
  return r;
}

/* one bit at a time, MSB first; reflection done on the data and result */
static uint32_t ref_crc(const CrcSpec *c, uint32_t init, const uint8_t *p,
                        size_t len) {
  uint64_t top = 1ULL << (c->width - 1U);
  uint64_t mask = (top << 1) - 1U;
  uint64_t crc = init;
  for (size_t i = 0U; i < len; i++) {
    uint32_t b = c->refl ? reflect(p[i], 8U) : p[i];
    crc ^= (uint64_t)b << (c->width - 8U);
    for (int k = 0; k < 8; k++) {
      crc = (crc & top) != 0U ? ((crc << 1) ^ c->poly) & mask
                              : (crc << 1) & mask;
    }
  }
  uint32_t r = (uint32_t)crc;
  return (c->refl ? reflect(r, c->width) : r) ^ c->xorout;
}

static uint32_t lib(const CrcSpec *c, uint32_t prev, const uint8_t *p,
                    size_t len) {
  if (c == &SMBUS) {
    return fu_crc8_smbus((uint8_t)prev, p, len);
  }
  if (c == &MAXIM) {
    return fu_crc8_maxim((uint8_t)prev, p, len);
  }
  if (c == &CCITT) {
    return fu_crc16_ccitt((uint16_t)prev, p, len);
  }
  if (c == &MODBUS) {
    return fu_crc16_modbus((uint16_t)prev, p, len);
  }
  return fu_crc32(prev, p, len);
}

static const CrcSpec *const all[] = {&SMBUS, &MAXIM, &CCITT, &MODBUS, &CRC32};
static const uint32_t lib_init[] = {FU_CRC8_SMBUS_INIT, FU_CRC8_MAXIM_INIT,
                                    FU_CRC16_CCITT_INIT, FU_CRC16_MODBUS_INIT,
                                    FU_CRC32_INIT};

void test_fu_crc_check_values(void) {
  const uint8_t *s = (const uint8_t *)"123456789";
  ASSERT_EQ(fu_crc8_smbus(FU_CRC8_SMBUS_INIT, s, 9U), 0xF4U);
  ASSERT_EQ(fu_crc8_maxim(FU_CRC8_MAXIM_INIT, s, 9U), 0xA1U);
  ASSERT_EQ(fu_crc16_ccitt(FU_CRC16_CCITT_INIT, s, 9U), 0x29B1U);
  ASSERT_EQ(fu_crc16_ccitt(0U, s, 9U), 0x31C3U); /* XMODEM */
  ASSERT_EQ(fu_crc16_modbus(FU_CRC16_MODBUS_INIT, s, 9U), 0x4B37U);
  ASSERT_EQ(fu_crc32(FU_CRC32_INIT, s, 9U), 0xCBF43926U);
  ASSERT_EQ(fu_crc32(FU_CRC32_INIT, NULL, 0U), 0U);
  for (size_t k = 0U; k < 5U; k++) { /* the reference agrees */
    ASSERT_EQ(ref_crc(all[k], all[k]->init, s, 9U),
              lib(all[k], lib_init[k], s, 9U));
  }
}

void test_fu_crc_vs_bitwise(void) {
  static uint8_t buf[512];
  uint64_t st = 10U;
  for (size_t i = 0U; i < sizeof buf; i++) {
    buf[i] = (uint8_t)test_rng_u32(&st);
  }
  for (int it = 0; it < 4000; it++) {
    uint32_t u = test_rng_u32(&st);
    size_t off = u & 7U; /* every alignment */
    size_t len = (u >> 3) % 300U;
    for (size_t k = 0U; k < 5U; k++) {
      ASSERT_EQ(lib(all[k], lib_init[k], buf + off, len),
                ref_crc(all[k], all[k]->init, buf + off, len));
    }
  }
}

void test_fu_crc_chaining(void) {
  uint8_t buf[67];
  for (size_t i = 0U; i < sizeof buf; i++) {
    buf[i] = (uint8_t)(i * 37U + 11U);
  }
  for (size_t k = 0U; k < 5U; k++) {
    uint32_t whole = lib(all[k], lib_init[k], buf, sizeof buf);
    for (size_t cut = 0U; cut <= sizeof buf; cut++) {
      uint32_t a = lib(all[k], lib_init[k], buf, cut);
      ASSERT_EQ(lib(all[k], a, buf + cut, sizeof buf - cut), whole);
    }
  }
}
