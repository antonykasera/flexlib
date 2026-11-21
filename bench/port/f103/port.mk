# bench board: STM32F103C8 Blue Pill clone, on flex

BENCH_CPU       = $(M3_FLAGS)
BENCH_TITLE     = STM32F103C8 (Blue Pill clone)
BENCH_REPORT   ?= usb
BENCH_OPENOCD  ?= -f interface/stlink.cfg -f target/stm32f1x.cfg
BENCH_PORT_DEFS = -DSTM32F103xB
BENCH_PORT_SRCS = $(BENCH_PORTDIR)/port.c $(BENCH_PORTDIR)/cache_probe.c
BENCH_GOLDEN    = bench/core/golden.c

ifneq ($(wildcard $(FLEX_DIR)/flex.mk),)
FLEX_MODULES := clock $(if $(filter usb,$(BENCH_REPORT)),gpio usb)
include $(FLEX_DIR)/flex.mk
BENCH_PORT_INC = $(FLEX_INC)
BENCH_EXT_SRCS = $(FLEX_SRCS)
BENCH_STARTUP  = $(FLEX_STARTUP)
else
BENCH_MISSING  = $(FLEX_DIR) is missing: make deps, or FLEX_DIR=../flex
endif
