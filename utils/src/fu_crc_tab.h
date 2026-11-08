/**
 * @file fu_crc_tab.h
 * @brief CRC tables (generated into fu_crc_tab.c)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FU_CRC_TAB_H
#define FU_CRC_TAB_H

#include "fx_arch.h"
#include <stdint.h>

/* byte tables, and nibble tables for FU_CRC_SMALL */
extern const uint8_t fu_crc8_smbus_tab[256], fu_crc8_smbus_nib[16];
extern const uint8_t fu_crc8_maxim_tab[256], fu_crc8_maxim_nib[16];
extern const uint16_t fu_crc16_ccitt_tab[256], fu_crc16_ccitt_nib[16];
extern const uint16_t fu_crc16_modbus_tab[256], fu_crc16_modbus_nib[16];

/* slice-by-4: [0] is the byte table, [k] the same after k more zero bytes */
extern const uint32_t fu_crc32_tab[4][256];
extern const uint32_t fu_crc32_nib[16];

#endif /* FU_CRC_TAB_H */
