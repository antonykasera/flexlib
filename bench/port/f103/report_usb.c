/**
 * @file report_usb.c
 * @brief Bench report over USB CDC (flex); the LED is on while enumerated
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench_port.h"
#include "board.h"
#include "hal_gpio.h"
#include "hal_usb.h"

#define LED_PORT GPIOC
#define LED_PIN 13U

void bench_report_init(void) {
  GPIO_Config led_cfg = {GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2_MHZ, GPIO_PULL_NONE};

  hal_gpio_init_port_clock(LED_PORT);
  hal_gpio_pin_init(LED_PORT, LED_PIN, &led_cfg);
  hal_gpio_pin_write(LED_PORT, LED_PIN, true); /* off (active low) */
  hal_usb_init();
}

void bench_report_wait(void) {
  static bool was_open;
  while (1) {
    hal_usb_poll();
    hal_gpio_pin_write(LED_PORT, LED_PIN, !hal_usb_configured());
    bool open = hal_usb_cdc_dtr();
    bool opened = open && !was_open;
    was_open = open;
    if (opened) {
      return;
    }
  }
}

bool bench_out(const char *s, size_t n) {
  while (n > 0U) {
    size_t k = hal_usb_cdc_write(s, n);
    s += k;
    n -= k;
    hal_usb_poll();
    if (!hal_usb_cdc_dtr()) {
      return false;
    }
  }
  return true;
}
