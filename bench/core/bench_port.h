/**
 * @file bench_port.h
 * @brief What the bench needs from a board port and a report backend
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BENCH_PORT_H
#define BENCH_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void bench_port_init(void);
void bench_port_describe(void); /* ", flash ..." after the CPUID */
void bench_port_extra(void);

/* report_usb.c (port) or report_rtt.c (core) */
void bench_report_init(void);
void bench_report_wait(void);
bool bench_out(const char *s, size_t n); /* false: reader gone */

/* bench_main.c */
void bench_print(const char *s);
void bench_print_u32(uint32_t v);
void bench_print_x10(uint32_t v);
void bench_print_hex32(uint32_t v);

#endif /* BENCH_PORT_H */
