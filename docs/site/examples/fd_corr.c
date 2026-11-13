#include <stdio.h>

#include "fd_corr.h"
#include "fd_noise.h"

int main(void) {
  /* b is a noise burst; a holds it again 37.25 samples later, made by
   * averaging delays of 37 and 38 */
  static fm_q15 b[256], a[320];
  FU_Xoshiro128 r;
  fu_xoshiro128_seed(&r, 7U);
  fd_noise_gauss_q15(&r, 4000, b, 256U);
  for (int i = 0; i < 320; i++) {
    int s37 = i >= 37 && i < 293 ? b[i - 37] : 0;
    int s38 = i >= 38 && i < 294 ? b[i - 38] : 0;
    a[i] = (fm_q15)((3 * s37 + s38) / 4);
  }
  /* lags 0 .. 63, scaled down by 2^8 (256 terms); the parabola through the
   * peak and its neighbours puts it at 37.11 */
  fm_q15 y[64];
  fd_corr_q15(a, 320U, b, 256U, 0, 64U, 8U, y);
  fm_q16 lag = fd_peak_q15(y, 64U, NULL);

  printf("%.2f\n", (double)lag / 65536.0);
  return lag > 37 * 65536 && lag < 38 * 65536 ? 0 : 1;
}
