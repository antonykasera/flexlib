#include <stdio.h>
#include <string.h>

#include "fm_sqrt.h"
#include "fu_fmt.h"

int main(void) {
  char buf[FU_FMT_Q16_SIZE];
  fm_q16 r = fm_sqrt_q16(2 * FM_Q16_ONE); /* sqrt(2) in Q16.16, rounded */
  fu_fmt_q16(buf, r, 5U);                 /* its exact value, 5 decimals */
  puts(buf);
  return strcmp(buf, "1.41422") == 0 ? 0 : 1;
}
