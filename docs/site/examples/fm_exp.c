#include <math.h>
#include <stdio.h>

#include "fm_exp.h"

/* error in LSB against a double reference */
static double err(fm_q16 got, double want) {
  return fabs((double)got - want * 65536.0);
}

int main(void) {
  fm_q16 tenth = FM_Q16_ONE / 10;
  /* an RMS level against full scale, in dB */
  fm_q16 db = fm_db_q16(tenth);
  /* an RC decay after one time constant */
  fm_q16 k = fm_exp_q16(-FM_Q16_ONE);
  /* x^y with a fractional exponent */
  fm_q16 r = fm_pow_q16(2 * FM_Q16_ONE, FM_Q16_ONE / 2);
  printf("%.4f %.4f %.4f\n", db / 65536.0, k / 65536.0, r / 65536.0);
  return err(db, 20.0 * log10(tenth / 65536.0)) <= 0.501 &&
                 err(k, exp(-1.0)) <= 0.54 && err(r, sqrt(2.0)) <= 0.66
             ? 0
             : 1;
}
