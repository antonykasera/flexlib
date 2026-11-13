#include <stdio.h>

#include "fm_fixed.h"

/* A q31 dot product: an exact Q62 sum, rounded once. */
static fm_q31 dot_q31(const fm_q31 *a, const fm_q31 *b, int n) {
  int64_t acc = 0;
  for (int i = 0; i < n; i++) {
    acc = fm_mac_q31(acc, a[i], b[i]);
  }
  return fm_q62_to_q31(acc);
}

int main(void) {
  fm_q31 a[3], b[3];
  for (int i = 0; i < 3; i++) {
    a[i] = fm_f32_to_q31(0.5f);
    b[i] = fm_f32_to_q31(0.25f * (float)(i + 1));
  }
  fm_q31 d = dot_q31(a, b, 3);                 /* 0.5 * (0.25 + 0.5 + 0.75) */
  fm_q15 m = fm_mul_q15(INT16_MIN, INT16_MIN); /* -1 * -1 saturates */
  printf("%f %d\n", (double)fm_q31_to_f32(d), m);
  return d == fm_f32_to_q31(0.75f) && m == INT16_MAX ? 0 : 1;
}
