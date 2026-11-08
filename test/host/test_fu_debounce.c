/**
 * @file test_fu_debounce.c
 * @brief Host tests for fu_debounce.h, against per-input references
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_debounce.h"
#include "test_stats.h"

/* each input bounces for a while after a change, then settles */
static uint32_t bouncy(uint32_t *level, uint32_t *noise, uint64_t *st) {
  uint32_t r = test_rng_u32(st);
  uint32_t change = test_rng_u32(st) & test_rng_u32(st) & test_rng_u32(st) &
                    test_rng_u32(st) & test_rng_u32(st); /* ~1/32 per tick */
  *level ^= change;
  *noise = (*noise | change) & (test_rng_u32(st) | test_rng_u32(st));
  return *level ^ (*noise & r);
}

void test_fu_debounce32_vs_reference(void) {
  FU_Debounce32 d;
  fu_debounce32_init(&d, 0x0F0F0F0FU);
  uint32_t state = 0x0F0F0F0FU;
  uint8_t cnt[32] = {0};
  uint32_t level = 0x0F0F0F0FU;
  uint32_t noise = 0U;
  uint64_t st = 101U;
  uint64_t flips = 0U;
  for (int t = 0; t < 1000000; t++) {
    uint32_t raw = bouncy(&level, &noise, &st);
    uint32_t want = 0U;
    for (unsigned b = 0U; b < 32U; b++) {
      if (((raw ^ state) >> b & 1U) != 0U) {
        if (++cnt[b] == 4U) {
          want |= 1U << b;
          cnt[b] = 0U;
        }
      } else {
        cnt[b] = 0U;
      }
    }
    state ^= want;
    uint32_t got = fu_debounce32(&d, raw);
    ASSERT_EQ(got, want);
    ASSERT_EQ(d.state, state);
    flips += (uint64_t)__builtin_popcount(want);
  }
  ASSERT_TRUE(flips > 100000U); /* inputs did change */
}

void test_fu_debounce_vs_reference(void) {
  for (uint8_t limit = 1U; limit <= 12U; limit++) {
    FU_Debounce d;
    fu_debounce_init(&d, limit, false);
    int count = 0;
    bool state = false;
    uint32_t level = 0U;
    uint32_t noise = 0U;
    uint64_t st = 200U + limit;
    for (int t = 0; t < 200000; t++) {
      bool raw = (bouncy(&level, &noise, &st) & 1U) != 0U;
      count = raw ? (count < limit ? count + 1 : count)
                  : (count > 0 ? count - 1 : count);
      bool before = state;
      if (count == limit) {
        state = true;
      } else if (count == 0) {
        state = false;
      }
      ASSERT_EQ(fu_debounce(&d, raw), state != before);
      ASSERT_EQ(d.state, state);
    }
  }
}

/* settled, then a glitch of up to limit - 1 samples: no change; a steady
 * change takes exactly limit samples */
void test_fu_debounce_glitch(void) {
  for (uint8_t limit = 1U; limit <= 12U; limit++) {
    for (uint8_t g = 0U; g < limit; g++) {
      FU_Debounce d;
      fu_debounce_init(&d, limit, false);
      for (uint8_t k = 0U; k < g; k++) {
        ASSERT_TRUE(!fu_debounce(&d, true));
      }
      for (uint8_t k = 0U; k < limit; k++) {
        ASSERT_TRUE(!fu_debounce(&d, false));
      }
      for (uint8_t k = 1U; k < limit; k++) {
        ASSERT_TRUE(!fu_debounce(&d, true));
      }
      ASSERT_TRUE(fu_debounce(&d, true));
      ASSERT_TRUE(d.state);
    }
  }
  /* starting high: it takes limit zeros to fall */
  FU_Debounce h;
  fu_debounce_init(&h, 5U, true);
  for (int k = 0; k < 4; k++) {
    ASSERT_TRUE(!fu_debounce(&h, false));
  }
  ASSERT_TRUE(fu_debounce(&h, false));
  ASSERT_TRUE(!h.state);

  FU_Debounce32 v;
  fu_debounce32_init(&v, 0U);
  ASSERT_EQ(fu_debounce32(&v, 1U), 0U);
  ASSERT_EQ(fu_debounce32(&v, 1U), 0U);
  ASSERT_EQ(fu_debounce32(&v, 0U), 0U); /* a match restarts the count */
  ASSERT_EQ(fu_debounce32(&v, 1U), 0U);
  ASSERT_EQ(fu_debounce32(&v, 1U), 0U);
  ASSERT_EQ(fu_debounce32(&v, 1U), 0U);
  ASSERT_EQ(fu_debounce32(&v, 1U), 1U);
  ASSERT_EQ(v.state, 1U);
}
