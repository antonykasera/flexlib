#include <stdio.h>

#include "fd_fir.h"

#define TAPS 31U

int main(void) {
  /* lowpass at 0.1 cycles per sample, Hamming window */
  float hf[TAPS];
  fm_q15 h[TAPS], st[FD_FIR_STATE_LEN(TAPS, 64U)];
  if (fd_fir_design(hf, TAPS, FD_LOWPASS, 0.1f, 0.0f, FD_WIN_HAMMING) != FD_OK ||
      fd_fir_quantize_q15(hf, h, TAPS) != FD_OK) {
    return 1;
  }
  FD_FirQ15 f;
  fd_fir_init_q15(&f, h, TAPS, st, FD_FIR_STATE_LEN(TAPS, 64U));

  /* DC 8192 plus a Nyquist tone of 8192: the filter keeps the DC */
  fm_q15 x[64], y[64];
  for (uint32_t i = 0U; i < 64U; i++) {
    x[i] = (i & 1U) != 0U ? 16384 : 0;
  }
  fd_fir_q15(&f, x, y, 64U);

  /* what is left of the tone: (y[62] - y[63]) / 2, about 4.5 LSB */
  printf("%d %d\n", y[62], y[63]);
  return y[62] - 8192 < 10 && 8192 - y[63] < 10 ? 0 : 1;
}
