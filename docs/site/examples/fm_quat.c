#include <math.h>
#include <stdio.h>

#include "fm_quat.h"

int main(void) {
  /* a gyro reads 90 degrees per second about z for 1 s, at 1 kHz */
  const float w[3] = {0.0f, 0.0f, 1.5707963f};
  FM_Quat q = {1.0f, 0.0f, 0.0f, 0.0f};
  for (int i = 0; i < 1000; i++) {
    fm_quat_integrate(&q, &q, w, 1e-3f);
  }
  /* the body's x axis now points along y */
  const float x[3] = {1.0f, 0.0f, 0.0f};
  float v[3];
  fm_quat_rotate(v, &q, x);

  printf("%.6f %.6f %.6f\n", (double)v[0], (double)v[1], (double)v[2]);
  return fabsf(v[0]) < 1e-5f && fabsf(v[1] - 1.0f) < 1e-5f &&
                 fabsf(v[2]) < 1e-6f
             ? 0
             : 1;
}
