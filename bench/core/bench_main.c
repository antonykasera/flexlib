/**
 * @file bench_main.c
 * @brief flexlib bench firmware: runs every case and prints the report
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "bench_port.h"
#include "board.h"
#include "fu_fmt.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef BENCH_OPT_NAME
#define BENCH_OPT_NAME "?"
#endif
#ifndef BENCH_SET_NAME
#define BENCH_SET_NAME "?"
#endif

static uint32_t cyc[BENCH_N];
static bool out_open;

/* stops printing once the reader has gone */
void bench_print(const char *s) {
  if (out_open) {
    out_open = bench_out(s, strlen(s));
  }
}

void bench_print_u32(uint32_t v) {
  char buf[FU_FMT_U32_SIZE];
  fu_fmt_u32(buf, v);
  bench_print(buf);
}

void bench_print_hex32(uint32_t v) {
  char buf[2U + FU_FMT_HEX32_SIZE] = "0x";
  fu_fmt_hex32(buf + 2, v, 8U, true);
  bench_print(buf);
}

void bench_print_x10(uint32_t v) {
  bench_print_u32(v / 10U);
  char frac[3] = {'.', (char)('0' + v % 10U), '\0'};
  bench_print(frac);
}

static void cycles_init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void print_row(const char *name, const char *impl, uint32_t min,
                      uint32_t median, uint32_t max, const char *golden) {
  bench_print("| ");
  bench_print(name);
  bench_print(" | ");
  bench_print(impl);
  bench_print(" | ");
  bench_print_u32(min);
  bench_print(" | ");
  bench_print_u32(median);
  bench_print(" | ");
  bench_print_u32(max);
  bench_print(" | ");
  bench_print(golden);
  bench_print(" |\r\n");
}

typedef struct {
  const BenchCase *c;
  BenchResult r;
} SlowCase;

static void print_slow(const SlowCase *s, unsigned n) {
  bench_print("\r\nslow samples (above 2 x median + 4), by input index:\r\n\r\n");
  bench_print("| case | samples | max | at input |\r\n");
  bench_print("|---|--:|--:|---|\r\n");
  for (unsigned k = 0U; k < n; k++) {
    bench_print("| ");
    bench_print(s[k].c->name);
    bench_print(" | ");
    bench_print_u32(s[k].r.n_slow);
    bench_print(" | ");
    bench_print_u32(s[k].r.max);
    bench_print(" | ");
    uint32_t listed = s[k].r.n_slow < BENCH_SLOW_LIST ? s[k].r.n_slow
                                                      : BENCH_SLOW_LIST;
    for (uint32_t j = 0U; j < listed; j++) {
      if (j > 0U) {
        bench_print(", ");
      }
      bench_print_u32(s[k].r.slow_at[j]);
    }
    bench_print(s[k].r.n_slow > BENCH_SLOW_LIST ? ", ... |\r\n" : " |\r\n");
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
  if (bench_cold_groups_n == 0U) {
    return;
  }
  __disable_irq();
  uint32_t cold_overhead = bench_overhead(cyc, true);
  __enable_irq();
  bench_print("\r\n### Cold calls\r\n\r\n");
  bench_print(BENCH_COLD_NOTE " Same inputs and overhead as above; the empty "
              "case cold reads ");
  bench_print_u32(cold_overhead);
  bench_print(".\r\n\r\n");
  bench_print("| case | impl | warm median | min | median | max | golden |\r\n");
  bench_print("|---|---|--:|--:|--:|--:|---|\r\n");
  for (unsigned g = 0U; g < bench_cold_groups_n && out_open; g++) {
    for (unsigned i = 0U; i < bench_cold_groups[g]->n && out_open; i++) {
      const BenchCase *c = &bench_cold_groups[g]->cases[i];
      const BenchCase *wc = find_case(c->name);
      __disable_irq();
      BenchResult w = bench_run(wc != NULL ? wc : c, overhead, cyc);
      BenchResult r = bench_run(c, overhead, cyc);
      __enable_irq();
      bench_print("| ");
      bench_print(c->name);
      bench_print(" | ");
      bench_print(c->impl);
      bench_print(" | ");
      bench_print_u32(w.median);
      bench_print(" | ");
      bench_print_u32(r.min);
      bench_print(" | ");
      bench_print_u32(r.median);
      bench_print(" | ");
      bench_print_u32(r.max);
      bench_print(" | ");
      bench_print(golden_str[bench_golden_check(c, r.hash)]);
      bench_print(" |\r\n");
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

  bench_print("<!-- bench begin -->\r\n");
  bench_print("## flexlib bench, " BENCH_SET_NAME " image, " BENCH_BOARD "\r\n\r\n");
  bench_print("- build: " BENCH_OPT_NAME ", GCC " __VERSION__ ", newlib-nano\r\n");
  bench_print("- CPUID ");
  bench_print_hex32(SCB->CPUID);
  bench_port_describe();
  bench_print("- cycles per call over ");
  bench_print_u32(BENCH_N);
  bench_print(" inputs, CYCCNT read cost (");
  bench_print_u32(overhead);
  bench_print(") subtracted; interrupts off while timing\r\n\r\n");

  bench_print("| case | impl | min | median | max | golden |\r\n");
  bench_print("|---|---|--:|--:|--:|---|\r\n");
  print_row("CYCCNT read (raw)", "-", overhead, overhead, overhead, "-");

  for (unsigned g = 0U; g < bench_groups_n && out_open; g++) {
    for (unsigned i = 0U; i < bench_groups[g]->n && out_open; i++) {
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

  bench_print("\r\ngolden: ");
  bench_print_u32(count[BENCH_GOLDEN_OK]);
  bench_print(" ok, ");
  bench_print_u32(count[BENCH_GOLDEN_FAIL]);
  bench_print(" FAIL, ");
  bench_print_u32(count[BENCH_GOLDEN_MISSING]);
  bench_print(" missing\r\n");
  run_cold(overhead, golden_str);
  bench_port_extra();
  bench_print("<!-- bench end -->\r\n");
}

int main(void) {
  bench_port_init();
  cycles_init();
  bench_inputs_init();
  bench_report_init();
  while (1) {
    bench_report_wait();
    out_open = true;
    run_all();
  }
}
