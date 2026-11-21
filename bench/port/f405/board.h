/**
 * @file board.h
 * @brief Bench board: STM32F405RG (WeAct), 8 MHz crystal
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f405xx.h"

#define BENCH_BOARD "STM32F405 @ 168 MHz"
#define BENCH_COLD_NOTE                                                        \
  "Each sample follows a reset of the ART accelerator's instruction and "      \
  "data caches, so the function's code and tables come from flash."

#endif /* BOARD_H */
