/**
 * @file board.h
 * @brief Bench board: STM32F103C8 Blue Pill
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f103xb.h"

#define BENCH_BOARD "STM32F103 @ 72 MHz"
#define BENCH_COLD_NOTE                                                        \
  "Each sample follows a flush of the flash cache (32 KB read from SRAM "      \
  "code), so the function's code and tables come from flash; the case's own "  \
  "loop is read back first."

#endif /* BOARD_H */
