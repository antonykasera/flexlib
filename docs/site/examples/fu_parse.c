#include <stdio.h>
#include <string.h>

#include "fu_parse.h"

int main(void) {
  const char *cmd = "12.5V";
  int32_t v;
  size_t used;
  /* a prefix parse: 12.5 in Q16.16, 4 characters, "V" left over */
  FU_ParseStatus st = fu_parse_q16(cmd, strlen(cmd), &v, &used);

  uint32_t big;
  /* a whole-string parse that overflows: saturated, FU_PARSE_RANGE */
  FU_ParseStatus r = fu_parse_u32("4294967296", 10U, &big, NULL);

  printf("%d %ld %u %d %lu\n", st, (long)v, (unsigned)used, r,
         (unsigned long)big);
  return st == FU_PARSE_OK && v == 819200 && used == 4U &&
                 r == FU_PARSE_RANGE
             ? 0
             : 1;
}
