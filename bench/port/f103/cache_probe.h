/**
 * @file cache_probe.h
 * @brief Flash read cost by block size and stride (bench board only)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CACHE_PROBE_H
#define CACHE_PROBE_H

#include <stdint.h>

#define CACHE_PROBE_SIZES 8U
#define CACHE_PROBE_STRIDES 5U

typedef struct {
  uint32_t bytes;    /* block size, or stride */
  uint32_t cold_x10; /* cycles per load x10, first pass after a flush */
  uint32_t warm_x10; /* second pass */
} CacheProbeRow;

typedef struct {
  uint32_t sram_x10; /* same loop over SRAM: the loop's own cost */
  CacheProbeRow size[CACHE_PROBE_SIZES];     /* one load per 32 bytes */
  CacheProbeRow stride[CACHE_PROBE_STRIDES]; /* 1 KB block */
} CacheProbe;

/* sram: any SRAM buffer, for the baseline. */
void cache_probe_run(CacheProbe *p, const void *sram, uint32_t sram_bytes);

#endif /* CACHE_PROBE_H */
