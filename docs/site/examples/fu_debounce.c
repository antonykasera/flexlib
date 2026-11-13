#include <stdio.h>

#include "fu_debounce.h"

int main(void) {
  FU_Debounce32 keys;
  fu_debounce32_init(&keys, 0U);

  /* key 0 bounces, then stays down; one sample per tick */
  const uint32_t raw[] = {1U, 0U, 1U, 1U, 1U, 1U, 1U};
  unsigned at = 0U;
  for (unsigned t = 0U; t < sizeof raw / sizeof raw[0]; t++) {
    uint32_t changed = fu_debounce32(&keys, raw[t]);
    if ((changed & keys.state) != 0U) {
      at = t; /* pressed */
    }
  }
  printf("pressed at tick %u\n", at);
  return at == 5U ? 0 : 1;
}
