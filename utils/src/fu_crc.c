/**
 * @file fu_crc.c
 * @brief Software CRCs: CRC-8, CRC-16 and CRC-32 (zlib)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_crc.h"
#include "fu_bits.h"
#include "fu_crc_tab.h"

/* The state runs in a uint32_t; MSB-first CRCs let stale high bits pile up
 * above the CRC width, and the return masks them off. */

#ifdef FU_CRC_SMALL
#define BYTES(step)                                                            \
  for (; len != 0U; len--) {                                                   \
    uint32_t b = *p++;                                                         \
    step;                                                                      \
  }
#else
/* bytes to the next aligned word, then 4 steps per word load */
#define BYTES(step)                                                            \
  size_t h = (0U - (uintptr_t)p) & 3U;                                         \
  for (h = h < len ? h : len, len -= h; h != 0U; h--) {                        \
    uint32_t b = *p++;                                                         \
    step;                                                                      \
  }                                                                            \
  for (; len >= 4U; len -= 4U, p += 4) {                                       \
    uint32_t w = fu_load_le32(p);                                              \
    uint32_t b = w & 0xFFU;                                                    \
    step;                                                                      \
    b = (w >> 8) & 0xFFU;                                                      \
    step;                                                                      \
    b = (w >> 16) & 0xFFU;                                                     \
    step;                                                                      \
    b = w >> 24;                                                               \
    step;                                                                      \
  }                                                                            \
  for (; len != 0U; len--) {                                                   \
    uint32_t b = *p++;                                                         \
    step;                                                                      \
  }
#endif

uint8_t fu_crc8_smbus(uint8_t crc, const void *data, size_t len) {
  FX_ASSERT(data != NULL || len == 0U);
  const uint8_t *p = data;
  uint32_t c = crc;
#ifdef FU_CRC_SMALL
  BYTES(c ^= b; c = (c << 4) ^ fu_crc8_smbus_nib[(c >> 4) & 0xFU];
        c = (c << 4) ^ fu_crc8_smbus_nib[(c >> 4) & 0xFU])
#else
  BYTES(c = fu_crc8_smbus_tab[c ^ b])
#endif
  return (uint8_t)c;
}

uint8_t fu_crc8_maxim(uint8_t crc, const void *data, size_t len) {
  FX_ASSERT(data != NULL || len == 0U);
  const uint8_t *p = data;
  uint32_t c = crc;
#ifdef FU_CRC_SMALL
  BYTES(c ^= b; c = (c >> 4) ^ fu_crc8_maxim_nib[c & 0xFU];
        c = (c >> 4) ^ fu_crc8_maxim_nib[c & 0xFU])
#else
  BYTES(c = fu_crc8_maxim_tab[c ^ b])
#endif
  return (uint8_t)c;
}

uint16_t fu_crc16_ccitt(uint16_t crc, const void *data, size_t len) {
  FX_ASSERT(data != NULL || len == 0U);
  const uint8_t *p = data;
  uint32_t c = crc;
#ifdef FU_CRC_SMALL
  BYTES(c = (c << 4) ^ fu_crc16_ccitt_nib[((c >> 12) ^ (b >> 4)) & 0xFU];
        c = (c << 4) ^ fu_crc16_ccitt_nib[((c >> 12) ^ b) & 0xFU])
#else
  BYTES(c = (c << 8) ^ fu_crc16_ccitt_tab[((c >> 8) ^ b) & 0xFFU])
#endif
  return (uint16_t)c;
}

uint16_t fu_crc16_modbus(uint16_t crc, const void *data, size_t len) {
  FX_ASSERT(data != NULL || len == 0U);
  const uint8_t *p = data;
  uint32_t c = crc;
#ifdef FU_CRC_SMALL
  BYTES(c ^= b; c = (c >> 4) ^ fu_crc16_modbus_nib[c & 0xFU];
        c = (c >> 4) ^ fu_crc16_modbus_nib[c & 0xFU])
#else
  BYTES(c = (c >> 8) ^ fu_crc16_modbus_tab[(c ^ b) & 0xFFU])
#endif
  return (uint16_t)c;
}

uint32_t fu_crc32(uint32_t crc, const void *data, size_t len) {
  FX_ASSERT(data != NULL || len == 0U);
  const uint8_t *p = data;
  uint32_t c = ~crc;
#ifdef FU_CRC_SMALL
  for (; len != 0U; len--) {
    c ^= *p++;
    c = (c >> 4) ^ fu_crc32_nib[c & 0xFU];
    c = (c >> 4) ^ fu_crc32_nib[c & 0xFU];
  }
#else
  const uint32_t *t0 = fu_crc32_tab[0], *t1 = fu_crc32_tab[1];
  const uint32_t *t2 = fu_crc32_tab[2], *t3 = fu_crc32_tab[3];
  for (; len >= 4U; len -= 4U, p += 4) {
    c ^= fu_load_le32(p);
    c = t3[c & 0xFFU] ^ t2[(c >> 8) & 0xFFU] ^ t1[(c >> 16) & 0xFFU] ^
        t0[c >> 24];
  }
  for (; len != 0U; len--) {
    c = (c >> 8) ^ t0[(c ^ *p++) & 0xFFU];
  }
#endif
  return ~c;
}
