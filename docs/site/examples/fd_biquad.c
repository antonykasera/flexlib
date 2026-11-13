#include <stdio.h>

#include "fd_biquad.h"

int main(void) {
  /* 4th-order Butterworth lowpass at 0.01 cycles per sample: 2 sections */
  FD_BiquadDesign d[2];
  fm_q31 c[10], st[FD_BIQUAD_STATE_LEN(2U)];
  uint32_t bshift;
  if (fd_biquad_butter(d, 4U, FD_LOWPASS, 0.01f) != FD_OK ||
      fd_biquad_quantize_q31(d, 2U, c, &bshift) != FD_OK) {
    return 1;
  }
  FD_BiquadQ31 f;
  fd_biquad_init_q31(&f, c, 2U, bshift, st);

  /* a step to 0.5: the output settles on it */
  fm_q31 x[100], y[100];
  for (uint32_t i = 0U; i < 100U; i++) {
    x[i] = 0x40000000;
  }
  for (int blk = 0; blk < 10; blk++) {
    fd_biquad_q31(&f, x, y, 100U);
  }
  /* the rounded coefficients move the DC gain by a few parts in 10^7 */
  long long err = (long long)y[99] - 0x40000000;
  printf("%u %lld\n", bshift, err);
  return err > -1000 && err < 1000 ? 0 : 1;
}
