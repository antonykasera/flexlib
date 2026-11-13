#include <stdio.h>

#include "fm_linalg.h"

int main(void) {
  /* a quarter turn about z in q31; 1 is the largest value, and
   * (2^31 - 1) 2^30 / 2^31 rounds up to 2^30 */
  static const fm_q31 rz[9] = {0, -INT32_MAX, 0, INT32_MAX, 0, 0,
                               0, 0,          INT32_MAX};
  const fm_q31 x[3] = {1 << 30, 0, 0}; /* 0.5 along x */
  fm_q31 y[3];
  fm_mat3_vec_q31(y, rz, x);

  /* one exact sum, rounded once: the 1 survives the cancelling terms */
  const float a[3] = {1e8f, 1.0f, -1e8f}, b[3] = {1.0f, 1.0f, 1.0f};
  float d = fm_dot3_f32(a, b);
  float plain = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];

  printf("%d %d %d, %g (plain float %g)\n", y[0], y[1], y[2], (double)d,
         (double)plain);
  return y[0] == 0 && y[1] == 1 << 30 && y[2] == 0 && d == 1.0f ? 0 : 1;
}
