#include <stdio.h>

#include "fu_fmt.h"

int main(void) {
  int32_t temp = 1546650; /* 23.6 in Q16.16 */
  char line[48];
  size_t n = fu_snprintf(line, sizeof line, "t=%.1q C id=%08X n=%u", temp,
                         0xBEEFU, 42U);

  char q[FU_FMT_Q16_SIZE];
  fu_fmt_q16(q, temp, 4U); /* "23.6000", as printf "%.4f" */

  puts(line);
  puts(q);
  return n == 25U ? 0 : 1;
}
