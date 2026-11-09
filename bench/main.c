/**
 * @file main.c
 * @brief flexlib bench firmware: cycle counts and golden checks over USB CDC
 *
 * Runs every time /dev/ttyACM0 opens (`make bench-run`); interrupts are off
 * while a case runs. PC13 LED on = enumerated.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "cache_probe.h"
#include "fu_fmt.h"
#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_usb.h"
#include "stm32f103xb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
  char buf[FU_FMT_U32_SIZE];
  fu_fmt_u32(buf, v);
  print(buf);
}

static void print_hex32(uint32_t v) {
  char buf[2U + FU_FMT_HEX32_SIZE] = "0x";
  fu_fmt_hex32(buf + 2, v, 8U, true);
  print(buf);
}

/* v / 10 with one decimal */
static void print_x10(uint32_t v) {
  print_u32(v / 10U);
  char frac[3] = {'.', (char)('0' + v % 10U), '\0'};
  print(frac);
}

static void print_probe_row(const char *label, uint32_t v, const char *unit,
                            const CacheProbeRow *r) {
  print("| ");
  print(label);
  print_u32(v);
  print(unit);
  print(" | ");
  print_x10(r->cold_x10);
  print(" | ");
  print_x10(r->warm_x10);
  print(" |\r\n");
}

static void print_cache_probe(void) {
  static CacheProbe p;
  cache_probe_run(&p, cyc, sizeof cyc);

  print("\r\n### Flash read cost\r\n\r\n");
  print("Cycles per 32-bit load from flash, loop included, first pass after "
        "a flush (cold) and second pass (warm). The same loop over SRAM: ");
  print_x10(p.sram_x10);
  print(".\r\n\r\n");

  print("| block, one load per 32 B | cold | warm |\r\n");
  print("|--:|--:|--:|\r\n");
  for (unsigned i = 0U; i < CACHE_PROBE_SIZES; i++) {
    uint32_t b = p.size[i].bytes;
    if (b < 1024U) {
      print_probe_row("", b, " B", &p.size[i]);
    } else {
      print_probe_row("", b / 1024U, " KB", &p.size[i]);
    }
  }

  print("\r\n| stride, 1 KB block | cold | warm |\r\n");
  print("|--:|--:|--:|\r\n");
  for (unsigned i = 0U; i < CACHE_PROBE_STRIDES; i++) {
    print_probe_row("", p.stride[i].bytes, " B", &p.stride[i]);
  }
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

typedef struct {
  const BenchCase *c;
  BenchResult r;
} SlowCase;

static void print_slow(const SlowCase *s, unsigned n) {
  print("\r\nslow samples (above 2 x median + 4), by input index:\r\n\r\n");
  print("| case | samples | max | at input |\r\n");
  print("|---|--:|--:|---|\r\n");
  for (unsigned k = 0U; k < n; k++) {
    print("| ");
    print(s[k].c->name);
    print(" | ");
    print_u32(s[k].r.n_slow);
    print(" | ");
    print_u32(s[k].r.max);
    print(" | ");
    uint32_t listed = s[k].r.n_slow < BENCH_SLOW_LIST ? s[k].r.n_slow
                                                      : BENCH_SLOW_LIST;
    for (uint32_t j = 0U; j < listed; j++) {
      if (j > 0U) {
        print(", ");
      }
      print_u32(s[k].r.slow_at[j]);
    }
    print(s[k].r.n_slow > BENCH_SLOW_LIST ? ", ... |\r\n" : " |\r\n");
  }
}

/* the warm case of the same name */
static const BenchCase *find_case(const char *name) {
  for (unsigned g = 0U; g < bench_groups_n; g++) {
    for (unsigned i = 0U; i < bench_groups[g]->n; i++) {
      const BenchCase *c = &bench_groups[g]->cases[i];
      if (strcmp(c->name, name) == 0) {
        return c;
      }
    }
  }
  return NULL;
}

static void run_cold(uint32_t overhead, const char *const golden_str[]) {
  __disable_irq();
  uint32_t cold_overhead = bench_overhead(cyc, true);
  __enable_irq();
  print("\r\n### Cold calls\r\n\r\n");
  print("Each sample follows a flush of the flash cache (32 KB read from SRAM "
        "code), so the function's code and tables come from flash; the case's "
        "own loop is read back first. Same inputs and overhead as above; the "
        "empty case cold reads ");
  print_u32(cold_overhead);
  print(".\r\n\r\n");
  print("| case | impl | warm median | min | median | max | golden |\r\n");
  print("|---|---|--:|--:|--:|--:|---|\r\n");
  for (unsigned g = 0U; g < bench_cold_groups_n && port_open; g++) {
    for (unsigned i = 0U; i < bench_cold_groups[g]->n && port_open; i++) {
      const BenchCase *c = &bench_cold_groups[g]->cases[i];
      const BenchCase *wc = find_case(c->name);
      __disable_irq();
      BenchResult w = bench_run(wc != NULL ? wc : c, overhead, cyc);
      BenchResult r = bench_run(c, overhead, cyc);
      __enable_irq();
      print("| ");
      print(c->name);
      print(" | ");
      print(c->impl);
      print(" | ");
      print_u32(w.median);
      print(" | ");
      print_u32(r.min);
      print(" | ");
      print_u32(r.median);
      print(" | ");
      print_u32(r.max);
      print(" | ");
      print(golden_str[bench_golden_check(c, r.hash)]);
      print(" |\r\n");
    }
  }
}

static void run_all(void) {
  static const char *const golden_str[] = {"ok", "**FAIL**", "missing", "-"};
  static SlowCase slow[32];
  unsigned n_slow = 0U;
  unsigned count[4] = {0U, 0U, 0U, 0U};

  __disable_irq();
  uint32_t overhead = bench_overhead(cyc, false);
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
      if (r.n_slow > 0U && n_slow < BENCH_COUNT(slow)) {
        slow[n_slow].c = c;
        slow[n_slow].r = r;
        n_slow++;
      }
    }
  }
  if (n_slow > 0U) {
    print_slow(slow, n_slow);
  }

  print("\r\ngolden: ");
  print_u32(count[BENCH_GOLDEN_OK]);
  print(" ok, ");
  print_u32(count[BENCH_GOLDEN_FAIL]);
  print(" FAIL, ");
  print_u32(count[BENCH_GOLDEN_MISSING]);
  print(" missing\r\n");
  run_cold(overhead, golden_str);
  print_cache_probe();
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
