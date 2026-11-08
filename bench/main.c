/**
 * @file main.c
 * @brief flexlib bench firmware: cycle counts and golden checks over USB CDC
 *
 * Plug in the Blue Pill's own USB port and open /dev/ttyACM0 (or run
 * `make bench-run`). Each time a terminal opens the port, every case runs and
 * a markdown table prints between "bench begin" / "bench end" markers.
 * PC13 LED on = enumerated.
 *
 * USB is polled and interrupts are off while a case runs, so no ISR lands in
 * a sample.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_usb.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef BENCH_OPT_NAME
#define BENCH_OPT_NAME "?"
#endif

#define LED_PORT GPIOC
#define LED_PIN 13U

void SystemInit(void) {}

static uint32_t cyc[BENCH_N];
static bool port_open;

/* Blocks until queued; gives up (and stops the run) if the port closes. */
static void print(const char *s) {
  size_t n = 0U;
  while (s[n] != '\0') {
    n++;
  }
  while (n > 0U && port_open) {
    size_t k = hal_usb_cdc_write(s, n);
    s += k;
    n -= k;
    hal_usb_poll();
    port_open = hal_usb_cdc_dtr();
  }
}

static void print_u32(uint32_t v) {
  char buf[11];
  uint32_t i = 10U;
  buf[i] = '\0';
  do {
    buf[--i] = (char)('0' + (v % 10U));
    v /= 10U;
  } while (v > 0U);
  print(&buf[i]);
}

static void print_hex32(uint32_t v) {
  char buf[11] = "0x";
  for (uint32_t i = 0U; i < 8U; i++) {
    buf[2U + i] = "0123456789ABCDEF"[(v >> (28U - 4U * i)) & 0xFU];
  }
  buf[10] = '\0';
  print(buf);
}

static void cycles_init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void print_row(const char *name, const char *impl, uint32_t min,
                      uint32_t median, uint32_t max, const char *golden) {
  print("| ");
  print(name);
  print(" | ");
  print(impl);
  print(" | ");
  print_u32(min);
  print(" | ");
  print_u32(median);
  print(" | ");
  print_u32(max);
  print(" | ");
  print(golden);
  print(" |\r\n");
}

static void run_all(void) {
  static const char *const golden_str[] = {"ok", "**FAIL**", "missing", "-"};
  unsigned count[4] = {0U, 0U, 0U, 0U};

  __disable_irq();
  uint32_t overhead = bench_overhead(cyc);
  __enable_irq();

  print("<!-- bench begin -->\r\n");
  print("## flexlib bench, STM32F103 @ 72 MHz\r\n\r\n");
  print("- build: " BENCH_OPT_NAME ", GCC " __VERSION__ ", newlib-nano\r\n");
  print("- CPUID ");
  print_hex32(SCB->CPUID);
  print(", flash ");
  print_u32(FLASH->ACR & FLASH_ACR_LATENCY_Msk);
  print((FLASH->ACR & FLASH_ACR_PRFTBS) != 0U ? " WS, prefetch on\r\n"
                                              : " WS, prefetch off\r\n");
  print("- cycles per call over ");
  print_u32(BENCH_N);
  print(" inputs, CYCCNT read cost (");
  print_u32(overhead);
  print(") subtracted; interrupts off while timing\r\n\r\n");

  print("| case | impl | min | median | max | golden |\r\n");
  print("|---|---|--:|--:|--:|---|\r\n");
  print_row("CYCCNT read (raw)", "-", overhead, overhead, overhead, "-");

  for (unsigned g = 0U; g < bench_groups_n && port_open; g++) {
    for (unsigned i = 0U; i < bench_groups[g]->n && port_open; i++) {
      const BenchCase *c = &bench_groups[g]->cases[i];
      __disable_irq();
      BenchResult r = bench_run(c, overhead, cyc);
      __enable_irq();
      BenchGoldenStatus st = bench_golden_check(c, r.hash);
      count[st]++;
      print_row(c->name, c->impl, r.min, r.median, r.max, golden_str[st]);
    }
  }

  print("\r\ngolden: ");
  print_u32(count[BENCH_GOLDEN_OK]);
  print(" ok, ");
  print_u32(count[BENCH_GOLDEN_FAIL]);
  print(" FAIL, ");
  print_u32(count[BENCH_GOLDEN_MISSING]);
  print(" missing\r\n");
  print("<!-- bench end -->\r\n");
}

int main(void) {
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};

  hal_sysclock_init(&CLOCK_CONFIG_72MHZ_HSE); /* USB needs the 72 MHz PLL */
  hal_systick_init();
  cycles_init();
  bench_inputs_init();

  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);
  hal_gpio_pin_write(LED_PORT, LED_PIN, true); /* off (active low) */

  hal_usb_init();

  bool was_open = false;
  while (1) {
    hal_usb_poll();
    hal_gpio_pin_write(LED_PORT, LED_PIN, !hal_usb_configured());

    port_open = hal_usb_cdc_dtr();
    if (port_open && !was_open) {
      run_all();
    }
    was_open = port_open;
  }
}
