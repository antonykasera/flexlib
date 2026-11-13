#include <math.h>
#include <stdio.h>

#include "fm_float.h"

int main(void) {
  /* polar form of (3, 4): exact length, angle within 0.545 ULP */
  float r = fm_sqrtf(3.0f * 3.0f + 4.0f * 4.0f);
  float a = fm_atan2f(4.0f, 3.0f);

  /* sin reduces any argument exactly: 10^6 rad is no harder than 1 */
  float s = fm_sinf(1e6f);
  double ref = sin(1e6);

  printf("%g %.7f %.7f %.7f\n", (double)r, (double)a, (double)s, ref);
  return r == 5.0f && fabs(a - atan2(4.0, 3.0)) < 1e-7 &&
                 fabs(s - ref) < 1e-7
             ? 0
             : 1;
}
