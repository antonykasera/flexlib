#include <stdio.h>

#include "fm_atan.h"

int main(void) {
  /* the phase of an I/Q sample; a full turn is 2^16 */
  int16_t ph = fm_atan2_q15(-16384, 16384);
  long deg = (long)ph * 360 / 65536;
  /* tilt from an accelerometer axis reading 0.5 g: 1/12 turn */
  int16_t tilt = fm_asin_q15(16384);
  /* no direction at the origin: 0 */
  int16_t none = fm_atan2_q15(0, 0);
  printf("%d %ld %d %d\n", ph, deg, tilt, none);
  return ph == -8192 && tilt == 5461 && none == 0 ? 0 : 1;
}
