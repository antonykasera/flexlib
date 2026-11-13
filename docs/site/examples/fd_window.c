#include <stdio.h>

#include "fd_window.h"

int main(void) {
  /* periodic Hann for a 256-point spectrum */
  static fm_q15 w[256];
  fd_window_q15(w, 256U, FD_WIN_HANN, true);
  fm_q16 coherent, power;
  fd_window_gains_q15(w, 256U, &coherent, &power);

  /* a tone's bin reads low by the coherent gain, noise by the power gain */
  printf("%d %d %.4f %.4f\n", w[0], w[128], (double)coherent / 65536.0,
         (double)power / 65536.0);
  return w[0] == 0 && w[128] == INT16_MAX && coherent - 32768 <= 1 &&
                 32768 - coherent <= 1 && power - 24576 <= 1 &&
                 24576 - power <= 1
             ? 0
             : 1;
}
