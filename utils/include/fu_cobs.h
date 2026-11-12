/**
 * @file fu_cobs.h
 * @brief COBS framing: zero-free packets, delimited by 0
 *
 * An encoded frame holds no 0 byte; a 0 on the wire ends each frame.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FU_COBS_H
#define FU_COBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Longest encoding of n bytes. */
#define FU_COBS_MAX(n) ((n) + (n) / 254U + 1U)

/**
 * Encode n bytes; returns the encoded length. The 0 that ends the frame is
 * not written.
 *
 * @param dst FU_COBS_MAX(n) bytes, not overlapping src
 * @param src data
 * @param n   bytes
 */
size_t fu_cobs_encode(void *dst, const void *src, size_t n);

/**
 * Decode one frame given without its ending 0. False if malformed (a 0
 * inside, or a block past the end); dst is then partly written.
 *
 * @param dst n bytes; may be src
 * @param src the frame
 * @param n   its length
 * @param len decoded length out
 */
bool fu_cobs_decode(void *dst, const void *src, size_t n, size_t *len);

/** Byte-at-a-time receiver. */
typedef struct {
  /** @privatesection */
  uint8_t *buf;
  size_t size;
  size_t len;
  uint32_t left; /* bytes left in block */
  uint8_t code;
  bool drop; /* skip to the next 0 */
  /** @publicsection */
  uint32_t errors; /**< frames dropped: malformed or larger than the buffer */
} FU_CobsRx;

/**
 * Set up a receiver.
 *
 * @param rx   receiver
 * @param buf  decoded frames; must outlive rx
 * @param size bytes at buf, the largest frame accepted
 */
void fu_cobs_rx_init(FU_CobsRx *rx, void *buf, size_t size);

/** Feed one byte. True on the 0 that ends a good frame: the frame is in
 * `buf[0, *len)` until the next call. Empty frames are skipped, not
 * counted. */
bool fu_cobs_rx_byte(FU_CobsRx *rx, uint8_t b, size_t *len);

#endif /* FU_COBS_H */
