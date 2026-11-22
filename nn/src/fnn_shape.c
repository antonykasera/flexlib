/**
 * @file fnn_shape.c
 * @brief Layers that only move values
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_shape.h"
#include <string.h>

static void copy(int8_t *d, const int8_t *s, uint32_t n) {
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

void fnn_concat_s8(const int8_t *const *x, const uint32_t *len, uint32_t n,
                   uint32_t outer, int8_t *y) {
  for (uint32_t r = 0U; r < outer; r++) {
    for (uint32_t i = 0U; i < n; i++) {
      copy(y, x[i] + r * len[i], len[i]);
      y += len[i];
    }
  }
}
