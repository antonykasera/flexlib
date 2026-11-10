# flexlib: host tests, Arm cross-builds, lint, asm-audit and the bench firmware.

BUILD = build

ARM_CC      = arm-none-eabi-gcc
ARM_OBJCOPY = arm-none-eabi-objcopy
ARM_OBJDUMP = arm-none-eabi-objdump
ARM_SIZE    = arm-none-eabi-size
HOST_CC    ?= gcc
PYTHON     ?= python3

# every library, through the same fragment consumers use
include flexlib.mk
LIB_SRCS = $(patsubst ./%,%,$(FLEXLIB_SRCS))
LIB_INC  = $(patsubst -I./%,-I%,$(FLEXLIB_CFLAGS))
# the libraries' private headers (fm_internal.h, fu_crc_tab.h), for tests and the bench
LIB_PRIV = -Iutils/src -Imath/src -Idsp/src

STD    = -std=c11
WARN   = -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
WERROR ?= -Werror

M3_FLAGS = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
M4_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
M7_FLAGS = -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16

all: check

check: host-test cross lint asm-audit

include mk/host.mk
include mk/accuracy.mk
include mk/cross.mk
include mk/lint.mk
include mk/bench.mk
include mk/tables.mk
include mk/release.mk

help:
	@echo "make [check]         host-test, cross, lint, asm-audit"
	@echo "make host-test       host tests, ASan + UBSan (HOST_CC=clang for the other compiler)"
	@echo "make exhaustive      every 32-bit input (exhaustive-slow: the text checks, ~13 min)"
	@echo "make accuracy        flexmath, flexdsp and CMSIS-DSP errors -> docs/accuracy.md"
	@echo "make cross           -Werror builds for Cortex-M3, M4, M7"
	@echo "make lint            -Wpedantic everywhere, each header on its own"
	@echo "make asm-audit       the M3 / M4 disassembly against test/asm/audit*.txt"
	@echo "make size            flash / RAM per library object (M3, -O2)"
	@echo "make golden          bench hashes on the host -> bench/core/golden.c"
	@echo "make bench           the bench images (BENCH_SET=utils|math|struct|dsp for one)"
	@echo "make bench-run       flash and read each image -> docs/results/f103.md"
	@echo "make bench-windows   the instructions each bench case times"
	@echo "make tables          regenerate the CRC and math tables"
	@echo "make deps            fetch flex and CMSIS-DSP (bench, accuracy, examples)"
	@echo "make dist            the release archive -> build/dist/flexlib-<version>.tar.gz"
	@echo "make consumer        a blank project builds against that archive"
	@echo "make clean"

# compile_commands.json for clangd (needs bear)
compdb:
	bear -- $(MAKE) -B cross host-test

clean:
	rm -rf $(BUILD)

.PHONY: all check help compdb clean
