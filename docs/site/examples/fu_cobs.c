#include <stdio.h>

#include "fu_cobs.h"

int main(void) {
  const uint8_t msg[] = {0x11, 0x00, 0x22};
  uint8_t wire[FU_COBS_MAX(sizeof msg) + 1U];
  size_t n = fu_cobs_encode(wire, msg, sizeof msg);
  wire[n++] = 0x00; /* end of frame */

  uint8_t buf[32];
  FU_CobsRx rx;
  fu_cobs_rx_init(&rx, buf, sizeof buf);
  size_t len = 0U;
  for (size_t i = 0U; i < n; i++) { /* as the bytes arrive */
    if (fu_cobs_rx_byte(&rx, wire[i], &len)) {
      printf("%u bytes: %02X %02X %02X\n", (unsigned)len, buf[0], buf[1],
             buf[2]);
    }
  }
  return len == 3U && buf[1] == 0x00U ? 0 : 1;
}
