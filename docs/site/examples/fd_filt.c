#include <stdio.h>

#include "fd_filt.h"

int main(void) {
  /* a sensor reading 1000 .. 1003 with one spike */
  fm_q15 x[40], m[40], y[40], buf[4];
  for (int32_t i = 0; i < 40; i++) {
    x[i] = (fm_q15)(i == 17 ? 32767 : 1000 + i % 4);
  }
  /* median of 5 removes the spike; the first and last two are copied */
  for (int32_t i = 0; i < 40; i++) {
    m[i] = i < 2 || i > 37 ? x[i]
                           : (fm_q15)fd_median5(x[i - 2], x[i - 1], x[i],
                                                x[i + 1], x[i + 2]);
  }
  /* then the mean of the last 4 */
  FD_MovAvgQ15 avg;
  fd_movavg_init_q15(&avg, buf, 4U);
  fd_movavg_q15(&avg, m, y, 40U);

  fm_q15 hi = 0;
  for (int i = 4; i < 38; i++) {
    hi = y[i] > hi ? y[i] : hi;
  }
  printf("%d %d\n", m[17], hi);
  return m[17] < 1004 && hi <= 1002 ? 0 : 1;
}
