#include <stdio.h>

#include "fd_fir.h"
#include "fd_lms.h"
#include "fd_noise.h"

int main(void) {
  /* an unknown 4-tap system, driven by noise */
  static const fm_q15 sys[4] = {9830, -6554, 3277, 1638};
  static fm_q15 x[4000], d[4000], fst[FD_FIR_STATE_LEN(4U, 4000U)];
  FU_Xoshiro128 r;
  fu_xoshiro128_seed(&r, 3U);
  fd_noise_gauss_q15(&r, 6000, x, 4000U);
  FD_FirQ15 plant;
  fd_fir_init_q15(&plant, sys, 4U, fst, FD_FIR_STATE_LEN(4U, 4000U));
  fd_fir_q15(&plant, x, d, 4000U);

  /* normalised LMS, mu = 0.25, learns it from x and d */
  int32_t w[4];
  fm_q15 st[FD_LMS_STATE_LEN(4U, 64U)], h[4];
  FD_LmsQ15 f;
  fd_lms_init_q15(&f, w, 4U, st, FD_LMS_STATE_LEN(4U, 64U), 8192,
                  FD_LMS_NORM);
  fd_lms_q15(&f, x, d, NULL, NULL, 4000U);
  fd_lms_coefs_q15(&f, h);

  int worst = 0;
  for (int k = 0; k < 4; k++) {
    int e = h[k] - sys[k];
    worst = e < 0 ? (-e > worst ? -e : worst) : (e > worst ? e : worst);
  }
  printf("%d %d %d %d, %d LSB\n", h[0], h[1], h[2], h[3], worst);
  return worst <= 2 ? 0 : 1;
}
