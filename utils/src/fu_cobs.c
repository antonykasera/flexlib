/**
 * @file fu_cobs.c
 * @brief COBS framing: zero-free packets, delimited by 0
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fu_cobs.h"
#include "fu_bits.h"

/* nonzero when some byte of w is 0 */
FX_INLINE uint32_t has_zero(uint32_t w) {
  return (w - 0x01010101U) & ~w & 0x80808080U;
}

size_t fu_cobs_encode(void *dst, const void *src, size_t n) {
  FX_ASSERT(dst != NULL && (src != NULL || n == 0U));
  uint8_t *d = dst;
  const uint8_t *s = src;
  size_t code_at = 0U;
  size_t o = 1U;
  uint32_t code = 1U;
  size_t i = 0U;
  while (i < n) {
    /* word path while code <= 0xFE; byte path closes full blocks */
    size_t words = (n - i) >> 2;
    size_t room = (0xFEU - code) >> 2;
    words = words < room ? words : room;
    if (words != 0U) {
      for (; words != 0U; words--, i += 4U, o += 4U, code += 4U) {
        uint32_t w = fu_load_le32(s + i);
        uint32_t z = has_zero(w);
        fu_store_le32(d + o, w);
        if (z != 0U) {
          uint32_t k = fx_ctz(z) >> 3;
          o += k;
          i += k + 1U;
          d[code_at] = (uint8_t)(code + k);
          code_at = o++;
          code = 1U;
          break;
        }
      }
      continue;
    }
    uint8_t b = s[i++];
    if (b != 0U) {
      d[o++] = b;
      if (++code != 0xFFU) {
        continue;
      }
      if (i == n) { /* full block at end: no final code */
        d[code_at] = 0xFFU;
        return o;
      }
    }
    d[code_at] = (uint8_t)code;
    code_at = o++;
    code = 1U;
  }
  d[code_at] = (uint8_t)code;
  return o;
}

bool fu_cobs_decode(void *dst, const void *src, size_t n, size_t *len) {
  FX_ASSERT(dst != NULL && len != NULL && (src != NULL || n == 0U));
  uint8_t *d = dst;
  const uint8_t *s = src;
  size_t i = 0U;
  size_t o = 0U;
  while (i < n) {
    uint32_t code = s[i++];
    if (code == 0U || code - 1U > n - i) {
      return false;
    }
    size_t k = code - 1U;
    /* d + o <= s + i: in-place safe */
    for (; k >= 4U; k -= 4U, i += 4U, o += 4U) {
      uint32_t w = fu_load_le32(s + i);
      if (has_zero(w) != 0U) {
        return false;
      }
      fu_store_le32(d + o, w);
    }
    for (; k != 0U; k--) {
      uint8_t b = s[i++];
      if (b == 0U) {
        return false;
      }
      d[o++] = b;
    }
    if (code != 0xFFU && i != n) {
      d[o++] = 0U;
    }
  }
  *len = o;
  return true;
}

void fu_cobs_rx_init(FU_CobsRx *rx, void *buf, size_t size) {
  FX_ASSERT(rx != NULL && buf != NULL);
  rx->buf = buf;
  rx->size = size;
  rx->len = 0U;
  rx->left = 0U;
  rx->code = 0U;
  rx->drop = false;
  rx->errors = 0U;
}

bool fu_cobs_rx_byte(FU_CobsRx *rx, uint8_t b, size_t *len) {
  uint32_t left = rx->left;
  size_t n = rx->len;
  if (b != 0U && left != 0U && n < rx->size) {
    rx->buf[n] = b;
    rx->len = n + 1U;
    rx->left = left - 1U;
    return false;
  }
  if (b == 0U) {
    bool good = !rx->drop && rx->left == 0U && rx->code != 0U;
    if (!good && (rx->drop || rx->left != 0U)) {
      rx->errors++; /* empty frames are resync, not errors */
    }
    *len = rx->len;
    rx->len = 0U;
    rx->left = 0U;
    rx->code = 0U;
    rx->drop = false;
    return good;
  }
  if (rx->drop) {
    return false;
  }
  if (left != 0U || (rx->code != 0U && rx->code != 0xFFU && n == rx->size)) {
    rx->drop = true; /* no room for a data byte or the implied 0 */
    return false;
  }
  if (rx->code != 0U && rx->code != 0xFFU) { /* previous block ended in 0 */
    rx->buf[n] = 0U;
    rx->len = n + 1U;
  }
  rx->code = b;
  rx->left = b - 1U;
  return false;
}
