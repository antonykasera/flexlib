# bench board: STM32F405RG, 8 MHz crystal, RTT only; ST and CMSIS 6 headers

CMSIS_CORE_DIR ?= third_party/cmsis-core
CMSIS_F4_DIR   ?= third_party/cmsis-device-f4

BENCH_CPU       = $(M4_FLAGS)
BENCH_TITLE     = STM32F405RG (WeAct)
BENCH_REPORT   ?= rtt
BENCH_OPENOCD  ?= -f interface/stlink.cfg -f target/stm32f4x.cfg
BENCH_PORT_DEFS = -DSTM32F405xx
BENCH_PORT_SRCS = $(BENCH_PORTDIR)/port.c
BENCH_GOLDEN    = bench/core/golden_fpu.c
BENCH_PORT_INC  = -isystem $(CMSIS_CORE_DIR)/CMSIS/Core/Include \
                  -isystem $(CMSIS_F4_DIR)/Include
BENCH_STARTUP   = $(CMSIS_F4_DIR)/Source/Templates/gcc/startup_stm32f405xx.s

ifneq ($(BENCH_REPORT),rtt)
$(error BENCH_TARGET=f405 reports over RTT only)
endif
ifeq ($(and $(wildcard $(CMSIS_CORE_DIR)/CMSIS/Core/Include/core_cm4.h),$(wildcard $(CMSIS_F4_DIR)/Include/stm32f405xx.h)),)
BENCH_MISSING = $(CMSIS_CORE_DIR) or $(CMSIS_F4_DIR) is missing: make deps
endif
