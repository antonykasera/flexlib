#include <stdio.h>

#include "fd_noise.h"
#include "fd_vec.h"

int main(void) {
  FU_Xoshiro128 r;
  fu_xoshiro128_seed(&r, 1U);

  /* Gaussian noise at sigma 0.1 (3277 LSB) */
  static fm_q15 y[10000];
  fd_noise_gauss_q15(&r, 3277, y, 10000U);
  fm_q15 sd = fd_std_q15(y, 10000U);

  printf("%d %d\n", fd_mean_q15(y, 10000U), sd);
  return sd > 3200 && sd < 3350 ? 0 : 1;
}
