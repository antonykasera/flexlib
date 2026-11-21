/**
 * @file report_rtt.c
 * @brief Bench report over SWD: a SEGGER RTT up-buffer the debugger reads
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench_port.h"

#define RTT_SIZE 512U
#define RTT_BLOCK_IF_FULL 2U

typedef struct {
  const char *name;
  char *buf;
  uint32_t size;
  volatile uint32_t wr;
  volatile uint32_t rd; /* the debugger's */
  uint32_t flags;
} RttBuffer;

/* SEGGER's control block layout; no down-buffers */
typedef struct {
  char id[16];
  int32_t n_up;
  int32_t n_down;
  RttBuffer up[1];
} RttControl;

static char rtt_buf[RTT_SIZE];

/* .data: valid before main; bench_run.py finds it by name */
RttControl bench_rtt = {
    "SEGGER RTT", 1, 0, {{"bench", rtt_buf, RTT_SIZE, 0U, 0U, RTT_BLOCK_IF_FULL}}};

void bench_report_init(void) {}

/* one run per reset */
void bench_report_wait(void) {
  static bool ran;
  if (ran) {
    for (;;) {
    }
  }
  ran = true;
}

/* blocks while the buffer is full */
bool bench_out(const char *s, size_t n) {
  RttBuffer *b = &bench_rtt.up[0];
  uint32_t wr = b->wr;
  while (n > 0U) {
    uint32_t next = wr + 1U == RTT_SIZE ? 0U : wr + 1U;
    if (next == b->rd) {
      continue;
    }
    rtt_buf[wr] = *s++;
    n--;
    wr = next;
    __asm volatile("dmb" ::: "memory"); /* byte before offset */
    b->wr = wr;
  }
  return true;
}
