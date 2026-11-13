#include <stdio.h>

#include "fd_resample.h"

#define TAPS 48U /* 16 per branch */

int main(void) {
  /* 32 kHz to 48 kHz: up 3, down 2 */
  float hf[TAPS];
  fm_q15 h[TAPS], st[FD_RESAMPLE_STATE_LEN(TAPS, 3U, 64U)];
  if (fd_resample_design(hf, TAPS, 3U, 2U, 0.9f, FD_WIN_BLACKMAN) != FD_OK ||
      fd_fir_quantize_q15(hf, h, TAPS) != FD_OK) {
    return 1;
  }
  FD_ResampleQ15 r;
  fd_resample_init_q15(&r, h, TAPS, 3U, 2U, st,
                       FD_RESAMPLE_STATE_LEN(TAPS, 3U, 64U));

  /* 64 inputs give 96 outputs; a constant comes out at its own level */
  fm_q15 x[64], y[FD_RESAMPLE_OUT_MAX(64U, 3U, 2U)];
  for (int i = 0; i < 64; i++) {
    x[i] = 10000;
  }
  uint32_t n = fd_resample_q15(&r, x, y, 64U);

  printf("%u %d\n", n, y[n - 1U]);
  return n == 96U && y[n - 1U] > 9990 && y[n - 1U] < 10010 ? 0 : 1;
}
