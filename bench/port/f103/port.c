/**
 * @file port.c
 * @brief Bench board: clock, flash, the flash read cost table
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "bench_port.h"
#include "board.h"
#include "cache_probe.h"
#include "hal_clock.h"

#if defined(BENCH_SET_utils) || defined(BENCH_SET_struct)
#define BENCH_CACHE_PROBE 1
#else
#define BENCH_CACHE_PROBE 0
#endif

void SystemInit(void) {}

void bench_port_init(void) {
  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE); /* USB needs the 72 MHz PLL */
  hal_systick_init();
}

void bench_port_describe(void) {
  bench_print(", flash ");
  bench_print_u32(FLASH->ACR & FLASH_ACR_LATENCY_Msk);
  bench_print((FLASH->ACR & FLASH_ACR_PRFTBS) != 0U ? " WS, prefetch on\r\n"
                                                    : " WS, prefetch off\r\n");
}

#if BENCH_CACHE_PROBE
static void print_probe_row(uint32_t v, const char *unit,
                            const CacheProbeRow *r) {
  bench_print("| ");
  bench_print_u32(v);
  bench_print(unit);
  bench_print(" | ");
  bench_print_x10(r->cold_x10);
  bench_print(" | ");
  bench_print_x10(r->warm_x10);
  bench_print(" |\r\n");
}

void bench_port_extra(void) {
  static CacheProbe p;
  cache_probe_run(&p, bench_arg_a, 1024U);

  bench_print("\r\n### Flash read cost\r\n\r\n");
  bench_print("Cycles per 32-bit load from flash, loop included, first pass "
              "after a flush (cold) and second pass (warm). The same loop over "
              "SRAM: ");
  bench_print_x10(p.sram_x10);
  bench_print(".\r\n\r\n");

  bench_print("| block, one load per 32 B | cold | warm |\r\n");
  bench_print("|--:|--:|--:|\r\n");
  for (unsigned i = 0U; i < CACHE_PROBE_SIZES; i++) {
    uint32_t b = p.size[i].bytes;
    if (b < 1024U) {
      print_probe_row(b, " B", &p.size[i]);
    } else {
      print_probe_row(b / 1024U, " KB", &p.size[i]);
    }
  }

  bench_print("\r\n| stride, 1 KB block | cold | warm |\r\n");
  bench_print("|--:|--:|--:|\r\n");
  for (unsigned i = 0U; i < CACHE_PROBE_STRIDES; i++) {
    print_probe_row(p.stride[i].bytes, " B", &p.stride[i]);
  }
}
#else
void bench_port_extra(void) {}
#endif
