/**
 * @file main.c
 * @brief A project using flexlib from the release archive (make consumer)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_fft.h"
#include "fd_vec.h"
#include "fm_float.h"
#include "fu_crc.h"
#include "fx_version.h"

_Static_assert(FLEXLIB_VERSION_NUM >= 100, "flexlib 0.1.0 or later");

static const char msg[] = "123456789";
static const fm_q15 sq[4] = {16384, -16384, 16384, -16384};
volatile float x = 0.5f;
volatile float y;
volatile uint32_t crc;
volatile fm_q15 rms;
static fm_q15 spec[8] __attribute__((aligned(4)));
volatile int32_t bin1; /* Re X[1] of an impulse: its height */

static void transform(void) {
  for (unsigned i = 0U; i < 8U; i++) {
    spec[i] = i == 0U ? 16384 : 0;
  }
  int32_t e = fd_rfft_q15(spec, 8U);
  bin1 = e >= 0 && e < 16 ? spec[2] * (1 << e) : 0;
}

#if defined(__arm__)
void SystemInit(void) {}

int main(void) {
  for (;;) {
    y = fm_sinf(x);
    crc = fu_crc32(FU_CRC32_INIT, msg, sizeof msg - 1U);
    rms = fd_rms_q15(sq, 4U);
    transform();
  }
}
#else
#include <stdio.h>

int main(void) {
  y = fm_sinf(x);
  crc = fu_crc32(FU_CRC32_INIT, msg, sizeof msg - 1U);
  rms = fd_rms_q15(sq, 4U);
  transform();
  printf("flexlib %s: sin(0.5) %.7f, crc32 %08x, rms %d, fft %d\n",
         FLEXLIB_VERSION, (double)y, (unsigned)crc, rms, (int)bin1);
  return y == 0x1.eaee88p-2f && crc == 0xCBF43926U && rms == 16384 &&
                 bin1 == 16384
             ? 0
             : 1;
}
#endif
