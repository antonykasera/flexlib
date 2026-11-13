#include <stdio.h>

#include "fu_ring.h"

static uint8_t mem[64];
static FU_Ring rx;

int main(void) {
  fu_ring_init(&rx, mem, sizeof mem);

  /* producer, e.g. a circular DMA: the hardware filled 5 bytes of dma[]
   * and its count register reads 11 */
  uint8_t dma[16] = "hello";
  size_t pos = 0U;
  fu_ring_from_dma(&rx, dma, sizeof dma, &pos, 11U);

  /* consumer, the main loop */
  char line[16];
  size_t n = fu_ring_read(&rx, line, sizeof line - 1U);
  line[n] = '\0';

  printf("%s %u %u\n", line, (unsigned)pos, (unsigned)fu_ring_used(&rx));
  return n == 5U ? 0 : 1;
}
