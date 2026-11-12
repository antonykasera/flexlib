/**
 * @file main.c
 * @brief A project using flexlib from the release archive (make consumer)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_corr.h"
#include "fd_fft.h"
#include "fd_lms.h"
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
volatile fm_q15 conv1, lms_y;

/* {1/2, -1/2} * {1/2} and one LMS sample from zero weights */
static void filters(void) {
  static int32_t w[2];
  static fm_q15 st[4], cv[2];
  const fm_q15 one = 16384, xs = 16384, ds = 8192;
  fd_conv_q15(sq, 2U, &one, 1U, 0U, cv);
  conv1 = cv[1];
  FD_LmsQ15 f;
  fm_q15 out;
  fd_lms_init_q15(&f, w, 2U, st, 4U, 16384, FD_LMS_PLAIN);
  fd_lms_q15(&f, &xs, &ds, &out, NULL, 1U);
  lms_y = out;
}

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
    filters();
  }
}
#else
#include <stdio.h>

int main(void) {
  y = fm_sinf(x);
  crc = fu_crc32(FU_CRC32_INIT, msg, sizeof msg - 1U);
  rms = fd_rms_q15(sq, 4U);
  transform();
  filters();
  printf("flexlib %s: sin(0.5) %.7f, crc32 %08x, rms %d, fft %d, conv %d, "
         "lms %d\n",
         FLEXLIB_VERSION, (double)y, (unsigned)crc, rms, (int)bin1, conv1,
         lms_y);
  return y == 0x1.eaee88p-2f && crc == 0xCBF43926U && rms == 16384 &&
                 bin1 == 16384 && conv1 == -8192 && lms_y == 0
             ? 0
             : 1;
}
#endif
