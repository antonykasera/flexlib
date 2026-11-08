/**
 * @file cases_crc.c
 * @brief Bench cases for fu_crc.h, with the table candidates, bitwise loops
 *        and the F1's CRC unit
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fu_bits.h"
#include "fu_crc.h"
#include "fu_crc_tab.h"

/* ---- CRC-32 candidates (fu_crc32: slice-by-4) ---- */

BENCH_CALL static uint32_t crc32_byte(const uint8_t *p, size_t len) {
  uint32_t c = 0xFFFFFFFFU;
  for (; len != 0U; len--) {
    c = (c >> 8) ^ fu_crc32_tab[0][(c ^ *p++) & 0xFFU];
  }
  return ~c;
}

BENCH_CALL static uint32_t crc32_nibble(const uint8_t *p, size_t len) {
  uint32_t c = 0xFFFFFFFFU;
  for (; len != 0U; len--) {
    c ^= *p++;
    c = (c >> 4) ^ fu_crc32_nib[c & 0xFU];
    c = (c >> 4) ^ fu_crc32_nib[c & 0xFU];
  }
  return ~c;
}

BENCH_CALL static uint32_t crc32_bitwise(const uint8_t *p, size_t len) {
  uint32_t c = 0xFFFFFFFFU;
  for (; len != 0U; len--) {
    c ^= *p++;
    for (unsigned k = 0U; k < 8U; k++) {
      c = (c >> 1) ^ (0xEDB88320U & (0U - (c & 1U)));
    }
  }
  return ~c;
}

/* The unit is CRC-32/MPEG-2 on words; bit-reversed in and out (and inverted)
 * it gives zlib's CRC (len % 4 == 0); the host runs fu_crc32 instead. */
#ifndef BENCH_HOST
#define CRC_DR (*(volatile uint32_t *)0x40023000U)
#define CRC_CR (*(volatile uint32_t *)0x40023008U)
#define RCC_AHBENR (*(volatile uint32_t *)0x40021014U)

BENCH_CALL static uint32_t crc32_hw(const uint8_t *p, size_t len) {
  CRC_CR = 1U; /* reset to 0xFFFFFFFF */
  for (; len != 0U; len -= 4U, p += 4) {
    CRC_DR = fx_rbit(fu_load_le32(p));
  }
  return ~fx_rbit(CRC_DR);
}
#else
#define crc32_hw(p, len) fu_crc32(FU_CRC32_INIT, (p), (len))
#endif

/* ---- CRC-16 / CRC-8: nibble tables (FU_CRC_SMALL) and bitwise ---- */

BENCH_CALL static uint32_t ccitt_nibble(const uint8_t *p, size_t len) {
  uint32_t c = 0xFFFFU;
  for (; len != 0U; len--) {
    uint32_t b = *p++;
    c = (c << 4) ^ fu_crc16_ccitt_nib[((c >> 12) ^ (b >> 4)) & 0xFU];
    c = (c << 4) ^ fu_crc16_ccitt_nib[((c >> 12) ^ b) & 0xFU];
  }
  return c & 0xFFFFU;
}

BENCH_CALL static uint32_t ccitt_bitwise(const uint8_t *p, size_t len) {
  uint32_t c = 0xFFFFU;
  for (; len != 0U; len--) {
    c ^= (uint32_t)*p++ << 8;
    for (unsigned k = 0U; k < 8U; k++) {
      c = (c << 1) ^ (0x1021U & (0U - ((c >> 15) & 1U)));
    }
  }
  return c & 0xFFFFU;
}

BENCH_CALL static uint32_t modbus_nibble(const uint8_t *p, size_t len) {
  uint32_t c = 0xFFFFU;
  for (; len != 0U; len--) {
    c ^= *p++;
    c = (c >> 4) ^ fu_crc16_modbus_nib[c & 0xFU];
    c = (c >> 4) ^ fu_crc16_modbus_nib[c & 0xFU];
  }
  return c;
}

BENCH_CALL static uint32_t modbus_bitwise(const uint8_t *p, size_t len) {
  uint32_t c = 0xFFFFU;
  for (; len != 0U; len--) {
    c ^= *p++;
    for (unsigned k = 0U; k < 8U; k++) {
      c = (c >> 1) ^ (0xA001U & (0U - (c & 1U)));
    }
  }
  return c;
}

BENCH_CALL static uint32_t smbus_nibble(const uint8_t *p, size_t len) {
  uint32_t c = 0U;
  for (; len != 0U; len--) {
    c ^= *p++;
    c = (c << 4) ^ fu_crc8_smbus_nib[(c >> 4) & 0xFU];
    c = (c << 4) ^ fu_crc8_smbus_nib[(c >> 4) & 0xFU];
  }
  return c & 0xFFU;
}

BENCH_CALL static uint32_t smbus_bitwise(const uint8_t *p, size_t len) {
  uint32_t c = 0U;
  for (; len != 0U; len--) {
    c ^= *p++;
    for (unsigned k = 0U; k < 8U; k++) {
      c = (c << 1) ^ (0x07U & (0U - ((c >> 7) & 1U)));
    }
  }
  return c & 0xFFU;
}

BENCH_CALL static uint32_t maxim_nibble(const uint8_t *p, size_t len) {
  uint32_t c = 0U;
  for (; len != 0U; len--) {
    c ^= *p++;
    c = (c >> 4) ^ fu_crc8_maxim_nib[c & 0xFU];
    c = (c >> 4) ^ fu_crc8_maxim_nib[c & 0xFU];
  }
  return c;
}

BENCH_CALL static uint32_t maxim_bitwise(const uint8_t *p, size_t len) {
  uint32_t c = 0U;
  for (; len != 0U; len--) {
    c ^= *p++;
    for (unsigned k = 0U; k < 8U; k++) {
      c = (c >> 1) ^ (0x8CU & (0U - (c & 1U)));
    }
  }
  return c;
}

/* ---- more candidates: aligned head, byte loops ---- */

/* the bytes before the next 4-aligned address, at most len */
static inline size_t head_len(const uint8_t *p, size_t len) {
  size_t h = (0U - (uintptr_t)p) & 3U;
  return h < len ? h : len;
}

BENCH_CALL static uint32_t crc32_s4_aligned(const uint8_t *p, size_t len) {
  const uint32_t *t0 = fu_crc32_tab[0], *t1 = fu_crc32_tab[1];
  const uint32_t *t2 = fu_crc32_tab[2], *t3 = fu_crc32_tab[3];
  uint32_t c = 0xFFFFFFFFU;
  size_t h = head_len(p, len);
  for (len -= h; h != 0U; h--) {
    c = (c >> 8) ^ t0[(c ^ *p++) & 0xFFU];
  }
  for (; len >= 4U; len -= 4U, p += 4) {
    c ^= fu_load_le32(p);
    c = t3[c & 0xFFU] ^ t2[(c >> 8) & 0xFFU] ^ t1[(c >> 16) & 0xFFU] ^
        t0[c >> 24];
  }
  for (; len != 0U; len--) {
    c = (c >> 8) ^ t0[(c ^ *p++) & 0xFFU];
  }
  return ~c;
}

/* byte tables, one byte per step (fu_crc16_* / fu_crc8_* unroll this by 4) */
BENCH_CALL static uint32_t ccitt_byte(const uint8_t *p, size_t len) {
  uint32_t c = 0xFFFFU;
  for (; len != 0U; len--) {
    c = (c << 8) ^ fu_crc16_ccitt_tab[((c >> 8) ^ *p++) & 0xFFU];
  }
  return c & 0xFFFFU;
}

BENCH_CALL static uint32_t modbus_byte(const uint8_t *p, size_t len) {
  uint32_t c = 0xFFFFU;
  for (; len != 0U; len--) {
    c = (c >> 8) ^ fu_crc16_modbus_tab[(c ^ *p++) & 0xFFU];
  }
  return c;
}

BENCH_CALL static uint32_t smbus_byte(const uint8_t *p, size_t len) {
  uint32_t c = 0U;
  for (; len != 0U; len--) {
    c = fu_crc8_smbus_tab[c ^ *p++];
  }
  return c;
}

BENCH_CALL static uint32_t maxim_byte(const uint8_t *p, size_t len) {
  uint32_t c = 0U;
  for (; len != 0U; len--) {
    c = fu_crc8_maxim_tab[c ^ *p++];
  }
  return c;
}

/* ---- inputs: an offset into fixed pseudo-random data ---- */

#define DATA_LEN 512U
static uint8_t *const data = bench_scratch;

static void data_init(void) {
  static const char id = 0;
  if (bench_scratch_claim(&id)) {
    uint32_t s = 0x2545F491U;
    for (size_t i = 0U; i < DATA_LEN; i++) {
      s ^= s << 13;
      s ^= s >> 17;
      s ^= s << 5;
      data[i] = (uint8_t)(s >> 24);
    }
  }
}

/* every alignment */
static inline uint32_t in_off256(uint32_t u) {
  data_init();
  return u % 256U;
}
static inline uint32_t in_off64(uint32_t u) {
  data_init();
  return u % 448U;
}
static inline uint32_t in_off8(uint32_t u) {
  data_init();
  return u % 504U;
}
static inline uint32_t in_off256_hw(uint32_t u) {
#ifndef BENCH_HOST
  RCC_AHBENR |= 1U << 6; /* CRCEN */
#endif
  return in_off256(u);
}

BENCH_UNARY(b_crc32_256, uint32_t, in_off256, uint32_t,
            fu_crc32(FU_CRC32_INIT, &data[x], 256U))
BENCH_UNARY(b_crc32_byte_256, uint32_t, in_off256, uint32_t,
            crc32_byte(&data[x], 256U))
BENCH_UNARY(b_crc32_nib_256, uint32_t, in_off256, uint32_t,
            crc32_nibble(&data[x], 256U))
BENCH_UNARY(b_crc32_bit_256, uint32_t, in_off256, uint32_t,
            crc32_bitwise(&data[x], 256U))
BENCH_UNARY(b_crc32_hw_256, uint32_t, in_off256_hw, uint32_t,
            crc32_hw(&data[x], 256U))
BENCH_UNARY(b_crc32_8, uint32_t, in_off8, uint32_t,
            fu_crc32(FU_CRC32_INIT, &data[x], 8U))
BENCH_UNARY(b_crc32_byte_8, uint32_t, in_off8, uint32_t,
            crc32_byte(&data[x], 8U))
BENCH_UNARY(b_crc32_nib_8, uint32_t, in_off8, uint32_t,
            crc32_nibble(&data[x], 8U))

BENCH_UNARY(b_ccitt, uint32_t, in_off64, uint32_t,
            fu_crc16_ccitt(FU_CRC16_CCITT_INIT, &data[x], 64U))
BENCH_UNARY(b_ccitt_nib, uint32_t, in_off64, uint32_t,
            ccitt_nibble(&data[x], 64U))
BENCH_UNARY(b_ccitt_bit, uint32_t, in_off64, uint32_t,
            ccitt_bitwise(&data[x], 64U))
BENCH_UNARY(b_modbus, uint32_t, in_off64, uint32_t,
            fu_crc16_modbus(FU_CRC16_MODBUS_INIT, &data[x], 64U))
BENCH_UNARY(b_modbus_nib, uint32_t, in_off64, uint32_t,
            modbus_nibble(&data[x], 64U))
BENCH_UNARY(b_modbus_bit, uint32_t, in_off64, uint32_t,
            modbus_bitwise(&data[x], 64U))
BENCH_UNARY(b_smbus, uint32_t, in_off64, uint32_t,
            fu_crc8_smbus(FU_CRC8_SMBUS_INIT, &data[x], 64U))
BENCH_UNARY(b_smbus_nib, uint32_t, in_off64, uint32_t,
            smbus_nibble(&data[x], 64U))
BENCH_UNARY(b_smbus_bit, uint32_t, in_off64, uint32_t,
            smbus_bitwise(&data[x], 64U))
BENCH_UNARY(b_maxim, uint32_t, in_off64, uint32_t,
            fu_crc8_maxim(FU_CRC8_MAXIM_INIT, &data[x], 64U))
BENCH_UNARY(b_maxim_nib, uint32_t, in_off64, uint32_t,
            maxim_nibble(&data[x], 64U))
BENCH_UNARY(b_maxim_bit, uint32_t, in_off64, uint32_t,
            maxim_bitwise(&data[x], 64U))

BENCH_UNARY(b_crc32_s4a_256, uint32_t, in_off256, uint32_t,
            crc32_s4_aligned(&data[x], 256U))
BENCH_UNARY(b_crc32_s4a_8, uint32_t, in_off8, uint32_t,
            crc32_s4_aligned(&data[x], 8U))
BENCH_UNARY(b_ccitt_byte, uint32_t, in_off64, uint32_t,
            ccitt_byte(&data[x], 64U))
BENCH_UNARY(b_modbus_byte, uint32_t, in_off64, uint32_t,
            modbus_byte(&data[x], 64U))
BENCH_UNARY(b_smbus_byte, uint32_t, in_off64, uint32_t,
            smbus_byte(&data[x], 64U))
BENCH_UNARY(b_maxim_byte, uint32_t, in_off64, uint32_t,
            maxim_byte(&data[x], 64U))

/* candidates of one CRC share a hash, so golden also checks them */
static const BenchCase cases[] = {
    {"fu_crc32, 256 B", "flexlib", b_crc32_256, true},
    {"crc32 byte table 1 KB, 256 B", "candidate", b_crc32_byte_256, true},
    {"crc32 nibble table (small), 256 B", "candidate", b_crc32_nib_256, true},
    {"crc32 bitwise, 256 B", "candidate", b_crc32_bit_256, true},
    {"crc32 F1 CRC unit + RBIT, 256 B", "hardware", b_crc32_hw_256, true},
    {"crc32 slice-by-4, aligned head, 256 B", "candidate", b_crc32_s4a_256,
     true},
    {"fu_crc32, 8 B", "flexlib", b_crc32_8, true},
    {"crc32 byte table 1 KB, 8 B", "candidate", b_crc32_byte_8, true},
    {"crc32 nibble table (small), 8 B", "candidate", b_crc32_nib_8, true},
    {"crc32 slice-by-4, aligned head, 8 B", "candidate", b_crc32_s4a_8, true},
    {"fu_crc16_ccitt, 64 B", "flexlib", b_ccitt, true},
    {"crc16 ccitt nibble table (small), 64 B", "candidate", b_ccitt_nib, true},
    {"crc16 ccitt bitwise, 64 B", "candidate", b_ccitt_bit, true},
    {"crc16 ccitt byte table, 1 per step, 64 B", "candidate", b_ccitt_byte,
     true},
    {"fu_crc16_modbus, 64 B", "flexlib", b_modbus, true},
    {"crc16 modbus nibble table (small), 64 B", "candidate", b_modbus_nib,
     true},
    {"crc16 modbus bitwise, 64 B", "candidate", b_modbus_bit, true},
    {"crc16 modbus byte table, 1 per step, 64 B", "candidate", b_modbus_byte,
     true},
    {"fu_crc8_smbus, 64 B", "flexlib", b_smbus, true},
    {"crc8 smbus nibble table (small), 64 B", "candidate", b_smbus_nib, true},
    {"crc8 smbus bitwise, 64 B", "candidate", b_smbus_bit, true},
    {"crc8 smbus byte table, 1 per step, 64 B", "candidate", b_smbus_byte,
     true},
    {"fu_crc8_maxim, 64 B", "flexlib", b_maxim, true},
    {"crc8 maxim nibble table (small), 64 B", "candidate", b_maxim_nib, true},
    {"crc8 maxim bitwise, 64 B", "candidate", b_maxim_bit, true},
    {"crc8 maxim byte table, 1 per step, 64 B", "candidate", b_maxim_byte,
     true},
};

const BenchGroup bench_group_crc = {cases, BENCH_COUNT(cases)};
