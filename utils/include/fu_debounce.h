/**
 * @file fu_debounce.h
 * @brief Debouncing: one input, or up to 32 at once
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FU_DEBOUNCE_H
#define FU_DEBOUNCE_H

#include "fx_arch.h"
#include <stdbool.h>
#include <stdint.h>

/* integrator: call once per tick with the raw sample */
typedef struct {
  uint8_t count;
  uint8_t limit;
  bool state;
} FU_Debounce;

FX_INLINE void fu_debounce_init(FU_Debounce *d, uint8_t samples, bool state) {
  FX_ASSERT(samples != 0U);
  d->limit = samples;
  d->count = state ? samples : 0U;
  d->state = state;
}

/* true when the state changed (new state in d->state) */
FX_INLINE bool fu_debounce(FU_Debounce *d, bool raw) {
  uint32_t c = d->count;
  if (raw) {
    c += c < d->limit ? 1U : 0U;
  } else {
    c -= c != 0U ? 1U : 0U;
  }
  d->count = (uint8_t)c;
  bool next = c == 0U ? false : (c == d->limit ? true : d->state);
  bool changed = next != d->state;
  d->state = next;
  return changed;
}

/* one bit per input; flips after 4 differing samples in a row */
typedef struct {
  uint32_t state;
  uint32_t ct0, ct1;
} FU_Debounce32;

FX_INLINE void fu_debounce32_init(FU_Debounce32 *d, uint32_t state) {
  d->state = state;
  d->ct0 = UINT32_MAX;
  d->ct1 = UINT32_MAX;
}

/* returns changed bits; presses = changed & state */
FX_INLINE uint32_t fu_debounce32(FU_Debounce32 *d, uint32_t raw) {
  uint32_t diff = raw ^ d->state;
  uint32_t ct0 = ~(d->ct0 & diff); /* a matching sample resets to 3 */
  uint32_t ct1 = ct0 ^ (d->ct1 & diff);
  uint32_t flip = diff & ct0 & ct1; /* counted 3 -> 0 and wrapped */
  d->ct0 = ct0;
  d->ct1 = ct1;
  d->state ^= flip;
  return flip;
}

#endif /* FU_DEBOUNCE_H */
