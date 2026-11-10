/**
 * @file main.c
 * @brief A project using flexlib from the release archive (make consumer)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_float.h"
#include "fu_crc.h"
#include "fx_version.h"

_Static_assert(FLEXLIB_VERSION_NUM >= 100, "flexlib 0.1.0 or later");

static const char msg[] = "123456789";
volatile float x = 0.5f;
volatile float y;
volatile uint32_t crc;

#if defined(__arm__)
void SystemInit(void) {}

int main(void) {
  for (;;) {
    y = fm_sinf(x);
    crc = fu_crc32(FU_CRC32_INIT, msg, sizeof msg - 1U);
  }
}
#else
#include <stdio.h>

int main(void) {
  y = fm_sinf(x);
  crc = fu_crc32(FU_CRC32_INIT, msg, sizeof msg - 1U);
  printf("flexlib %s: sin(0.5) %.7f, crc32 %08x\n", FLEXLIB_VERSION, (double)y,
         (unsigned)crc);
  return y == 0x1.eaee88p-2f && crc == 0xCBF43926U ? 0 : 1;
}
#endif
