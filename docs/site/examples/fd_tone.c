#include <math.h>
#include <stdio.h>

#include "fd_tone.h"

/* phase step for f Hz at 8 kHz */
#define STEP(f) (uint32_t)((f) * 4294967296.0 / 8000.0)

int main(void) {
  /* DTMF '5' (770 + 1336 Hz), each tone at quarter scale, 205 samples */
  FD_Nco lo, hi;
  fd_nco_init(&lo, 0U, STEP(770.0), 0);
  fd_nco_init(&hi, 0U, STEP(1336.0), 0);
  fm_q15 a[205], b[205], x[205];
  fd_nco_q15(&lo, a, NULL, 205U);
  fd_nco_q15(&hi, b, NULL, 205U);
  for (int i = 0; i < 205; i++) {
    x[i] = (fm_q15)((a[i] + b[i]) / 4);
  }

  /* one detector per row frequency: 770 Hz is present, 852 Hz is not */
  FD_Goertzel g770, g852;
  fd_goertzel_init(&g770, STEP(770.0), 0U);
  fd_goertzel_init(&g852, STEP(852.0), 0U);
  fd_goertzel_q15(&g770, x, 205U);
  fd_goertzel_q15(&g852, x, 205U);
  uint64_t p770 = fd_goertzel_power(&g770), p852 = fd_goertzel_power(&g852);

  double db = 10.0 * log10((double)p770 / (double)p852);
  printf("%.1f dB\n", db);
  return db > 20.0 ? 0 : 1;
}
