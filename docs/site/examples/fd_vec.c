#include <stdio.h>

#include "fd_vec.h"

int main(void) {
  /* a half-scale square wave, 64 samples */
  fm_q15 x[64];
  for (uint32_t i = 0U; i < 64U; i++) {
    x[i] = (i & 8U) != 0U ? 16384 : -16384;
  }
  uint32_t at;
  fm_q15 peak = fd_absmax_q15(x, 64U, &at);
  fm_q15 rms = fd_rms_q15(x, 64U);
  fm_q16 db;
  fd_db_q15(&rms, &db, 1U); /* 20 log10(1/2) = -6.0206 dB */

  printf("%d %u %d %.4f\n", peak, at, rms, (double)db / 65536.0);
  return peak == 16384 && at == 0U && rms == 16384 && db < -394500 &&
                 db > -394600
             ? 0
             : 1;
}
