/**
 * @file port.c
 * @brief Bench board: FPU, 168 MHz from the 8 MHz crystal, ART, cold flush
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "bench_port.h"
#include "board.h"

#define ART_EN (FLASH_ACR_ICEN | FLASH_ACR_DCEN)

void SystemInit(void) {
  SCB->CPACR |= (3UL << 20) | (3UL << 22); /* FPU */
  __DSB();
  __ISB();
}

/* 8 MHz / 8 * 336 / 2 = 168 MHz; Q 7: 48 MHz */
void bench_port_init(void) {
  RCC->CR |= RCC_CR_HSEON;
  while ((RCC->CR & RCC_CR_HSERDY) == 0U) {
  }
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  PWR->CR |= PWR_CR_VOS; /* scale 1: above 144 MHz */

  RCC->PLLCFGR = (8U << RCC_PLLCFGR_PLLM_Pos) | (336U << RCC_PLLCFGR_PLLN_Pos) |
                 (0U << RCC_PLLCFGR_PLLP_Pos) | RCC_PLLCFGR_PLLSRC_HSE |
                 (7U << RCC_PLLCFGR_PLLQ_Pos);
  RCC->CR |= RCC_CR_PLLON;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0U) {
  }

  FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN | ART_EN;
  RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2 |
              RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
  }
}

void bench_port_describe(void) {
  uint32_t acr = FLASH->ACR;
  bench_print(", rev ");
  bench_print_hex32(DBGMCU->IDCODE >> DBGMCU_IDCODE_REV_ID_Pos);
  bench_print(", flash ");
  bench_print_u32(acr & FLASH_ACR_LATENCY_Msk);
  bench_print((acr & FLASH_ACR_PRFTEN) != 0U ? " WS, prefetch on"
                                             : " WS, prefetch off");
  bench_print((acr & ART_EN) == ART_EN ? ", ART caches on\r\n"
                                       : ", ART caches off\r\n");
}

void bench_port_extra(void) {}

/* a cache is reset only while disabled */
void bench_cold_flush(void) {
  uint32_t acr = FLASH->ACR;
  FLASH->ACR = acr & ~ART_EN;
  FLASH->ACR = (acr & ~ART_EN) | FLASH_ACR_ICRST | FLASH_ACR_DCRST;
  FLASH->ACR = acr & ~ART_EN;
  FLASH->ACR = acr;
}
