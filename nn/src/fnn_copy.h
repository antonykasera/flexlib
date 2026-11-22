/**
 * @file fnn_copy.h
 * @brief Copies and fills by word, shared by the layers that move bytes
 *
 * newlib-nano's memcpy and memset go a byte at a time.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_COPY_H
#define FNN_COPY_H

#include "fx_arch.h"
#include <stdint.h>
#include <string.h>

/* Copies a word at a time, unaligned if need be, then the bytes left. */
FX_INLINE void fnn_copy(int8_t *d, const int8_t *s, uint32_t n) {
  uint32_t i = 0U;
  for (; i + 4U <= n; i += 4U) {
    uint32_t v;
    memcpy(&v, s + i, 4U);
    memcpy(d + i, &v, 4U);
  }
  for (; i < n; i++) {
    d[i] = s[i];
  }
}

/* Fills a word at a time, the word hidden so that GCC does not call memset. */
FX_INLINE void fnn_fill(int8_t *d, int32_t v, uint32_t n) {
  uint32_t w = (uint32_t)(uint8_t)v * 0x01010101U, i = 0U;
  for (; i + 4U <= n; i += 4U) {
    FX_OPAQUE(w);
    memcpy(d + i, &w, 4U);
  }
  for (; i < n; i++) {
    FX_OPAQUE(w);
    d[i] = (int8_t)w;
  }
}

#endif /* FNN_COPY_H */
