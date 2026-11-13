#include <stdio.h>

#include "fm_sqrt.h"

/* RMS of q15 samples: the mean square in Q30, then one integer root. */
static fm_q15 rms_q15(const fm_q15 *x, uint32_t n) {
  uint64_t ss = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    ss += (uint64_t)(x[i] * x[i]);
  }
  return (fm_q15)fm_isqrt64(ss / n);
}

int main(void) {
  const fm_q15 x[4] = {16384, -16384, 16384, -16384}; /* a square wave, 0.5 */
  fm_q15 r = rms_q15(x, 4U);
  fm_q16 k = fm_rsqrt_q16(4 * FM_Q16_ONE); /* 1 / sqrt(4) */
  fm_q15 z = fm_sqrt_q15(-1);              /* x <= 0 gives 0 */
  printf("%d %ld %d\n", r, (long)k, z);
  return r == 16384 && k == FM_Q16_ONE / 2 && z == 0 ? 0 : 1;
}
