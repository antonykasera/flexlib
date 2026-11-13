#include <stdio.h>

#include "fu_rand.h"

int main(void) {
  FU_Xoshiro128 a;
  fu_xoshiro128_seed(&a, 42U);
  FU_Xoshiro128 b = a;
  fu_xoshiro128_jump(&b); /* a second stream that does not overlap a */

  uint32_t die = fu_xoshiro128_below(&a, 6U) + 1U; /* 1 to 6, no bias */
  printf("%lu %08lX\n", (unsigned long)die,
         (unsigned long)fu_xoshiro128ss(&b));
  return die >= 1U && die <= 6U ? 0 : 1;
}
