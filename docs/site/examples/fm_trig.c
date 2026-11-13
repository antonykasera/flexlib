#include <math.h>
#include <stdio.h>

#include "fm_trig.h"

int main(void) {
  /* 1 kHz at 48 kHz: the phase steps by 2^32 * 1000 / 48000 */
  const uint32_t step = 89478485U;
  uint32_t phase = 0U;
  double worst = 0.0;
  for (int i = 0; i < 480; i++) { /* ten periods; the phase wraps itself */
    fm_q31 s, c;
    fm_sincos_q31(phase, &s, &c);
    double a = (double)phase * (6.283185307179586 / 4294967296.0);
    worst = fmax(worst, fabs((double)s - sin(a) * 2147483648.0));
    worst = fmax(worst, fabs((double)c - cos(a) * 2147483648.0));
    phase += step;
  }
  /* a quarter turn: sin is 1, saturated to the largest q15 */
  fm_q15 q = fm_sin_q15(0x4000U);
  printf("%.3f %d\n", worst, q);
  return worst <= 1.11 && q == INT16_MAX ? 0 : 1;
}
