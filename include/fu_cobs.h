/**
 * @file fu_cobs.h
 * @brief COBS framing: zero-free packets, delimited by 0
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Frames exclude the 0 delimiter: send fu_cobs_encode's output, then a 0.
 */

#ifndef FU_COBS_H
#define FU_COBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* encoded size of n bytes, at most */
#define FU_COBS_MAX(n) ((n) + (n) / 254U + 1U)

/* dst holds FU_COBS_MAX(n) and doesn't overlap src; returns the length */
size_t fu_cobs_encode(void *dst, const void *src, size_t n);

/* dst holds n (decoded frames are shorter) and may be src itself. false:
 * malformed (a 0 inside, or a block running past the end). */
bool fu_cobs_decode(void *dst, const void *src, size_t n, size_t *len);

/* Byte-at-a-time receiver, e.g. in a UART ISR. A frame too long for buf or
 * cut short is dropped and counted in errors. */
typedef struct {
  uint8_t *buf;
  size_t size;
  size_t len;
  uint32_t left; /* bytes to go in the current block */
  uint8_t code;  /* the current block's code; 0 before the first */
  bool drop;     /* this frame is bad: skip to the next 0 */
  uint32_t errors;
} FU_CobsRx;

void fu_cobs_rx_init(FU_CobsRx *rx, void *buf, size_t size);

/* true when b is the 0 that ends a good frame: buf[0..*len) holds it until
 * the next call */
bool fu_cobs_rx_byte(FU_CobsRx *rx, uint8_t b, size_t *len);

#endif /* FU_COBS_H */
