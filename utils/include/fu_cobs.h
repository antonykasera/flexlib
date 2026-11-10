/**
 * @file fu_cobs.h
 * @brief COBS framing: zero-free packets, delimited by 0
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FU_COBS_H
#define FU_COBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* encoded size of n bytes, at most */
#define FU_COBS_MAX(n) ((n) + (n) / 254U + 1U)

/* dst holds FU_COBS_MAX(n), no overlap; returns length (send a 0 after) */
size_t fu_cobs_encode(void *dst, const void *src, size_t n);

/* dst holds n, may be src; false if malformed */
bool fu_cobs_decode(void *dst, const void *src, size_t n, size_t *len);

/* byte-at-a-time receiver; bad or oversize frames are dropped, counted */
typedef struct {
  uint8_t *buf;
  size_t size;
  size_t len;
  uint32_t left; /* bytes left in block */
  uint8_t code;
  bool drop;     /* skip to the next 0 */
  uint32_t errors;
} FU_CobsRx;

void fu_cobs_rx_init(FU_CobsRx *rx, void *buf, size_t size);

/* true on the 0 ending a good frame; buf[0..*len) valid until next call */
bool fu_cobs_rx_byte(FU_CobsRx *rx, uint8_t b, size_t *len);

#endif /* FU_COBS_H */
