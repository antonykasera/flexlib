/**
 * @file cache_probe.c
 * @brief Flash read cost by block size and stride, cold and warm, to find the
 *        size and line of any flash cache on the bench board
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "cache_probe.h"
#include "stm32f103xb.h"

/* Probe the upper 32 KB (no code there runs during the probe); reading the
 * lower 32 KB evicts it. */
#define PROBE_BASE (FLASH_BASE + 0x8000U)
#define FLUSH_BASE FLASH_BASE
#define PROBE_SPAN 0x8000U

/* The timed loop runs from SRAM (.data, copied by the startup), so its own
 * instruction fetches don't go through the flash being probed. */
#define RAMFUNC __attribute__((section(".data.ramfunc"), noinline, long_call))

static volatile uint32_t sink;

static inline __attribute__((always_inline)) uint32_t
touch(uintptr_t base, uint32_t bytes, uint32_t stride) {
  uint32_t sum = 0U;
  for (uint32_t off = 0U; off < bytes; off += stride) {
    sum += *(volatile const uint32_t *)(base + off);
  }
  return sum;
}

/* cycles for one pass */
RAMFUNC static uint32_t timed(uintptr_t base, uint32_t bytes, uint32_t stride) {
  uint32_t t0 = DWT->CYCCNT;
  uint32_t sum = touch(base, bytes, stride);
  uint32_t t1 = DWT->CYCCNT;
  sink ^= sum;
  return t1 - t0;
}

/* cycles per load x10 */
static uint32_t timed_x10(uintptr_t base, uint32_t bytes, uint32_t stride) {
  return (timed(base, bytes, stride) * 10U) / (bytes / stride);
}

static void flush(void) { (void)timed(FLUSH_BASE, PROBE_SPAN, 32U); }

void cache_probe_run(CacheProbe *p, const void *sram, uint32_t sram_bytes) {
  static const uint32_t sizes[CACHE_PROBE_SIZES] = {256U,  512U,   1024U,
                                                    2048U, 4096U,  8192U,
                                                    16384U, 32768U};
  static const uint32_t strides[CACHE_PROBE_STRIDES] = {4U, 8U, 16U, 32U, 64U};

  __disable_irq();
  p->sram_x10 = timed_x10((uintptr_t)sram, sram_bytes, 32U);

  for (unsigned i = 0U; i < CACHE_PROBE_SIZES; i++) {
    flush();
    p->size[i].bytes = sizes[i];
    p->size[i].cold_x10 = timed_x10(PROBE_BASE, sizes[i], 32U);
    p->size[i].warm_x10 = timed_x10(PROBE_BASE, sizes[i], 32U);
  }

  for (unsigned i = 0U; i < CACHE_PROBE_STRIDES; i++) {
    flush();
    p->stride[i].bytes = strides[i];
    p->stride[i].cold_x10 = timed_x10(PROBE_BASE, 1024U, strides[i]);
    p->stride[i].warm_x10 = timed_x10(PROBE_BASE, 1024U, strides[i]);
  }
  __enable_irq();
}
