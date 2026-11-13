#include <stdio.h>

#include "fm_pid.h"

int main(void) {
  /* PI control of an RC stage (tau 100 steps) from a 0..1 output that a
   * 3.3 V PWM drives; setpoint 1.65 V */
  FM_PidQ16 p;
  fm_pid_init_q16(&p, 3 * FM_Q16_ONE / 2, 983, 0, 0, FM_Q16_ONE);
  fm_pid_reset_q16(&p, 0);
  const fm_q16 sp = 108134; /* 1.65 in Q16.16 */
  double v = 0.0, peak = 0.0;
  for (int k = 0; k < 2000; k++) {
    fm_q16 u = fm_pid_q16(&p, sp, (fm_q16)(v * 65536.0));
    v += ((double)u / 65536.0 * 3.3 - v) * 0.01;
    peak = v > peak ? v : peak;
  }
  /* the integrator stops at the limit while the output is clamped: no
   * overshoot */
  printf("%.4f V, peak %.4f V\n", v, peak);
  return v > 1.649 && v < 1.651 && peak < 1.66 ? 0 : 1;
}
