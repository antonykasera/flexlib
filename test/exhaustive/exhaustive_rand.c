/**
 * @file exhaustive_rand.c
 * @brief fu_xorshift32 visits every nonzero 32-bit state once per period
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_rand.h"
#include <stdio.h>

int main(void) {
  FU_Xorshift32 r;
  fu_xorshift32_seed(&r, 1U);
  unsigned long long steps = 0U;
  do {
    steps++;
  } while (fu_xorshift32(&r) != 1U && steps <= 0xFFFFFFFFULL);
  printf("exhaustive: fu_xorshift32 period %llu (want 4294967295)\n", steps);
  return steps == 0xFFFFFFFFULL ? 0 : 1;
}
