#include <stdio.h>

#include "fu_bits.h"

int main(void) {
  /* a big-endian frame: a 16-bit id, then a 12-bit signed reading */
  const uint8_t frame[] = {0x12, 0x34, 0x0F, 0x9C};
  uint16_t id = fu_load_be16(frame);
  int32_t t = fu_sext(fu_load_be16(frame + 2), 12U); /* 0xF9C is -100 */

  uint8_t out[4];
  fu_store_le32(out, 0xDEADBEEFU); /* any alignment */

  printf("%04X %ld %02X %lu\n", id, (long)t, out[0],
         (unsigned long)fu_popcount(0xF0F0U));
  return id == 0x1234U && t == -100 && out[0] == 0xEFU ? 0 : 1;
}
