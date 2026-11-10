/**
 * @file fu_crc.h
 * @brief Software CRCs: CRC-8, CRC-16 and CRC-32 (zlib)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FU_CRC_H
#define FU_CRC_H

#include <stddef.h>
#include <stdint.h>

/* first block: the _INIT value; to continue: the previous result */
#define FU_CRC8_SMBUS_INIT 0x00U
#define FU_CRC8_MAXIM_INIT 0x00U     /* Dallas 1-Wire */
#define FU_CRC16_CCITT_INIT 0xFFFFU  /* CCITT-FALSE; 0 gives XMODEM */
#define FU_CRC16_MODBUS_INIT 0xFFFFU
#define FU_CRC32_INIT 0x00000000U    /* as zlib's crc32(0, ...) */

uint8_t fu_crc8_smbus(uint8_t crc, const void *data, size_t len);
uint8_t fu_crc8_maxim(uint8_t crc, const void *data, size_t len);
uint16_t fu_crc16_ccitt(uint16_t crc, const void *data, size_t len);
uint16_t fu_crc16_modbus(uint16_t crc, const void *data, size_t len);
uint32_t fu_crc32(uint32_t crc, const void *data, size_t len);

#endif /* FU_CRC_H */
