#include <stdio.h>

#include "fm_div.h"

int main(void) {
  /* two counts below 2^15: their ratio in Q15 */
  fm_q15 duty = fm_div_q15(1200, 4800);
  /* Q16.16: 10 V over 3 ohm, rounded to nearest */
  fm_q16 amps = fm_div_q16(10 * FM_Q16_ONE, 3 * FM_Q16_ONE);
  /* by 0: saturates toward the dividend's sign */
  fm_q15 sat = fm_div_q15(-100, 0);
  printf("%d %ld %d\n", duty, (long)amps, sat);
  return duty == 8192 && amps == 218453 && sat == INT16_MIN ? 0 : 1;
}
