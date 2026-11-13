#include <math.h>
#include <stdio.h>

#include "fd_fft.h"
#include "fd_vec.h"
#include "fd_window.h"

int main(void) {
  /* a full-scale sine in bin 32 of 512 */
  static fm_q15 x[512] __attribute__((aligned(4))), w[512];
  for (int i = 0; i < 512; i++) {
    x[i] = (fm_q15)lrint(32767.0 * sin(6.283185307179586 * 32.0 * i / 512.0));
  }
  /* spectrum in dB: Hann window, real FFT, the exponent folded in */
  fm_q16 db[256];
  fd_window_q15(w, 512U, FD_WIN_HANN, true);
  fd_mul_q15(x, w, x, 512U);
  int32_t e = fd_rfft_q15(x, 512U);
  fd_fft_db_q15((fd_cq15 *)(void *)x, e, db, 256U); /* db[0] mixes DC, n/2 */

  /* 20 log10(n / 2) less the window's 6.02 dB: 42.14 */
  uint32_t k = 0U;
  for (uint32_t i = 1U; i < 256U; i++) {
    k = db[i] > db[k] ? i : k;
  }
  double peak = (double)db[k] / 65536.0;
  printf("%u %.2f\n", k, peak);
  return k == 32U && fabs(peak - 42.14) < 0.01 ? 0 : 1;
}
