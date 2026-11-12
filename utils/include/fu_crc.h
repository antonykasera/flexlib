/**
 * @file fu_crc.h
 * @brief Software CRCs: CRC-8, CRC-16 and CRC-32 (zlib)
 *
 * Each function takes the running CRC: the matching `_INIT` value for the
 * first piece of a message, the previous result for the rest. `data` may be
 * NULL when `len` is 0. `FU_CRC_SMALL` selects 16-entry tables.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FU_CRC_H
#define FU_CRC_H

#include <stddef.h>
#include <stdint.h>

/** Start value for fu_crc8_smbus(). */
#define FU_CRC8_SMBUS_INIT 0x00U
/** Start value for fu_crc8_maxim(). */
#define FU_CRC8_MAXIM_INIT 0x00U
/** Start value for fu_crc16_ccitt(); 0 gives XMODEM. */
#define FU_CRC16_CCITT_INIT 0xFFFFU
/** Start value for fu_crc16_modbus(). */
#define FU_CRC16_MODBUS_INIT 0xFFFFU
/** Start value for fu_crc32(). */
#define FU_CRC32_INIT 0x00000000U

/** CRC-8/SMBUS: poly 0x07, not reflected, no final XOR. `"123456789"` gives
 * 0xF4. */
uint8_t fu_crc8_smbus(uint8_t crc, const void *data, size_t len);

/** CRC-8/MAXIM (1-Wire): poly 0x31, reflected, no final XOR. `"123456789"`
 * gives 0xA1. */
uint8_t fu_crc8_maxim(uint8_t crc, const void *data, size_t len);

/** CRC-16/CCITT-FALSE: poly 0x1021, not reflected, no final XOR. `"123456789"`
 * gives 0x29B1 (XMODEM: 0x31C3). */
uint16_t fu_crc16_ccitt(uint16_t crc, const void *data, size_t len);

/** CRC-16/MODBUS: poly 0x8005, reflected, no final XOR. `"123456789"` gives
 * 0x4B37. Modbus RTU sends the low byte first. */
uint16_t fu_crc16_modbus(uint16_t crc, const void *data, size_t len);

/** CRC-32 (zlib, Ethernet, PNG): poly 0x04C11DB7, reflected, start and final
 * XOR 0xFFFFFFFF applied inside, as zlib's `crc32()`. `"123456789"` gives
 * 0xCBF43926. */
uint32_t fu_crc32(uint32_t crc, const void *data, size_t len);

#endif /* FU_CRC_H */
