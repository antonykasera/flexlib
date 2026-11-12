#include <stdio.h>

#include "fu_crc.h"

/* Modbus RTU: the CRC goes after the frame, low byte first. */
static size_t modbus_seal(uint8_t *f, size_t n) {
  uint16_t crc = fu_crc16_modbus(FU_CRC16_MODBUS_INIT, f, n);
  f[n] = (uint8_t)crc;
  f[n + 1U] = (uint8_t)(crc >> 8);
  return n + 2U;
}

int main(void) {
  uint8_t f[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  size_t n = modbus_seal(f, 6U);
  /* the receiver: frame and CRC together give 0 */
  int ok = fu_crc16_modbus(FU_CRC16_MODBUS_INIT, f, n) == 0U;

  /* CRC-32 of a message that arrives in two pieces */
  uint32_t c = fu_crc32(FU_CRC32_INIT, "12345", 5U);
  c = fu_crc32(c, "6789", 4U);

  printf("%02X %02X %d %08lX\n", f[6], f[7], ok, (unsigned long)c);
  return ok && c == 0xCBF43926U ? 0 : 1;
}
