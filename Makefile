# flexlib: host tests, Arm cross-builds, lint, asm-audit and the bench firmware.
#
# `make` (= `make check`) runs everything that needs neither the board nor the
# submodules. The bench needs bench/flex, and bench/cmsis-dsp for the CMSIS
# baselines; FLEX_DIR=../flex points it at a flex checkout instead.

BUILD = build

ARM_CC      = arm-none-eabi-gcc
ARM_OBJCOPY = arm-none-eabi-objcopy
ARM_OBJDUMP = arm-none-eabi-objdump
ARM_SIZE    = arm-none-eabi-size
HOST_CC    ?= gcc
PYTHON     ?= python3

# flexmath.mk pulls in flexutils.mk
include flexmath.mk
LIB_SRCS = $(patsubst ./%,%,$(FU_SRCS) $(FM_SRCS))
LIB_INC  = $(patsubst -I./%,-I%,$(FLEXLIB_CFLAGS))

STD    = -std=c11
WARN   = -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
WERROR ?= -Werror

M3_FLAGS = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
M4_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
M7_FLAGS = -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16

all: check

check: host-test cross lint asm-audit

# ---- host tests ----

HOST_BUILD  = $(BUILD)/host-$(HOST_CC)
HOST_CFLAGS = $(STD) $(WARN) $(WERROR) -O2 -g \
              -fsanitize=address,undefined -fno-sanitize-recover=undefined \
              -fno-omit-frame-pointer $(LIB_INC) -Itest/host
TEST_SRCS   = $(wildcard test/host/*.c) $(LIB_SRCS)

host-test:
	@mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_SRCS) -lm -o $(HOST_BUILD)/test_runner
	./$(HOST_BUILD)/test_runner
	$(HOST_CC) $(HOST_CFLAGS) -DFU_CRC_SMALL $(TEST_SRCS) -lm \
		-o $(HOST_BUILD)/test_runner_crc_small
	./$(HOST_BUILD)/test_runner_crc_small fu_crc
	@echo "--- Host tests passed ($(HOST_CC)) ---"

# ---- exhaustive: every 32-bit input, on all cores ----
# exhaustive-slow runs the *_slow.c checks (~13 min), kept out of CI

EXHAUSTIVE_ALL  = $(wildcard test/exhaustive/*.c)
EXHAUSTIVE_SLOW = $(filter %_slow.c,$(EXHAUSTIVE_ALL))

define RUN_EXHAUSTIVE
	@mkdir -p $(BUILD)/exhaustive
	@set -e; for t in $(1); do \
		b=$(BUILD)/exhaustive/$$(basename $$t .c); \
		$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -fopenmp $(LIB_INC) $$t \
			$(LIB_SRCS) -lm -o $$b; \
		echo "--- $$t"; ./$$b; \
	done
endef

exhaustive:
	$(call RUN_EXHAUSTIVE,$(filter-out $(EXHAUSTIVE_SLOW),$(EXHAUSTIVE_ALL)))

exhaustive-slow:
	$(call RUN_EXHAUSTIVE,$(EXHAUSTIVE_SLOW))

# ---- accuracy: flexmath and CMSIS-DSP against a double reference ----

ACC_BUILD = $(BUILD)/accuracy
ACC_CMSIS = $(addprefix bench/cmsis-dsp/Source/, \
              BasicMathFunctions/arm_mult_q15.c \
              BasicMathFunctions/arm_mult_q31.c \
              SupportFunctions/arm_float_to_q15.c \
              SupportFunctions/arm_q31_to_float.c \
              FastMathFunctions/arm_sqrt_q15.c \
              FastMathFunctions/arm_sqrt_q31.c \
              FastMathFunctions/arm_sin_q15.c \
              FastMathFunctions/arm_sin_q31.c \
              FastMathFunctions/arm_divide_q15.c \
              FastMathFunctions/arm_divide_q31.c \
              BasicMathFunctions/arm_abs_q15.c \
              BasicMathFunctions/arm_abs_q31.c \
              CommonTables/arm_common_tables.c)
ACC_CMSIS_INC = -isystem bench/cmsis-dsp/Include \
                -isystem bench/cmsis-dsp/PrivateInclude -isystem $(FLEX_DIR)/cmsis/inc

accuracy:
	@mkdir -p $(ACC_BUILD)
	@for f in $(ACC_CMSIS); do \
		$(HOST_CC) -O2 -w $(ACC_CMSIS_INC) -c $$f \
			-o $(ACC_BUILD)/$$(basename $$f .c).o || exit 1; \
	done
	$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 $(LIB_INC) -Itest/host $(ACC_CMSIS_INC) \
		test/accuracy/accuracy.c test/host/test_stats.c $(LIB_SRCS) \
		$(patsubst %.c,$(ACC_BUILD)/%.o,$(notdir $(ACC_CMSIS))) -lm \
		-o $(ACC_BUILD)/accuracy
	./$(ACC_BUILD)/accuracy > docs/accuracy.md

# ---- Arm cross-builds: M3 is the target; M4/M7 keep those arch paths building ----

ARM_CFLAGS = $(STD) $(WARN) $(WERROR) -O2 -g -ffunction-sections -fdata-sections \
             $(LIB_INC)
CROSS_CPUS = m3 m4 m7
CROSS_SRCS = $(LIB_SRCS) $(wildcard test/asm/*.c)
CROSS_OBJS = $(foreach c,$(CROSS_CPUS),$(patsubst %.c,$(BUILD)/cross/$(c)/%.o,$(CROSS_SRCS)))
M3_OBJS    = $(filter $(BUILD)/cross/m3/%,$(CROSS_OBJS))

define CROSS_RULE
$(BUILD)/cross/$(1)/%.o: %.c
	@mkdir -p $$(dir $$@)
	$(ARM_CC) $(2) $(ARM_CFLAGS) -MMD -MP -c $$< -o $$@
endef
$(eval $(call CROSS_RULE,m3,$(M3_FLAGS)))
$(eval $(call CROSS_RULE,m4,$(M4_FLAGS)))
$(eval $(call CROSS_RULE,m7,$(M7_FLAGS)))

cross: $(CROSS_OBJS)

# disassembly check of the -O2 M3 objects, see test/asm/audit.txt
asm-audit: $(M3_OBJS)
	$(PYTHON) tools/asm_audit.py --objdump $(ARM_OBJDUMP) --spec test/asm/audit.txt $^

# flash/RAM per library object (M3, -O2)
size: $(M3_OBJS)
	@objs="$(filter-out $(BUILD)/cross/m3/test/%,$^)"; \
	if [ -n "$$objs" ]; then $(ARM_SIZE) -t $$objs; else echo "no library objects yet"; fi

# ---- lint: -Wpedantic over everything, and each header on its own ----

LINT_FLAGS   = $(STD) $(WARN) -Wpedantic -Werror -fsyntax-only
LINT_HEADERS = $(wildcard include/*.h arch/*.h utils/src/*.h) bench/bench.h
BENCH_PORTABLE = bench/bench_core.c bench/cases_arch.c bench/cases_bits.c \
                 bench/cases_fmt.c bench/cases_crc.c bench/cases_ring.c \
                 bench/cases_parse.c bench/cases_cobs.c bench/cases_debounce.c \
                 bench/cases_rand.c bench/cases_fixed.c bench/cases_sqrt.c \
                 bench/cases_div.c bench/cases_trig.c \
                 bench/cases_libm.c

lint:
	$(ARM_CC) $(M3_FLAGS) $(LINT_FLAGS) $(LIB_INC) -Ibench -Iutils/src -Imath/src $(CROSS_SRCS) \
		$(BENCH_PORTABLE) bench/golden.c
	$(HOST_CC) $(LINT_FLAGS) $(LIB_INC) -Itest/host $(wildcard test/host/*.c)
	$(HOST_CC) $(LINT_FLAGS) -DBENCH_HOST $(LIB_INC) -Ibench -Iutils/src -Imath/src \
		$(BENCH_PORTABLE) bench/golden_gen.c
	$(ARM_CC) $(M3_FLAGS) $(LINT_FLAGS) -DFU_CRC_SMALL $(LIB_INC) utils/src/fu_crc.c
	$(ARM_CC) $(M3_FLAGS) $(LINT_FLAGS) -DFM_LUT_IN_RAM $(LIB_INC) math/src/fm_tables.c
	@for h in $(LINT_HEADERS); do \
		echo "  header $$h"; \
		echo "#include \"$$h\"" | $(ARM_CC) $(M3_FLAGS) $(LINT_FLAGS) $(LIB_INC) \
			-I. -x c - || exit 1; \
	done

# ---- golden hashes: the bench cases run on the host ----

GOLDEN_BUILD = $(BUILD)/golden

golden:
	@mkdir -p $(GOLDEN_BUILD)
	$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -DBENCH_HOST $(LIB_INC) -Ibench -Iutils/src -Imath/src \
		bench/golden_gen.c $(BENCH_PORTABLE) $(LIB_SRCS) -lm \
		-o $(GOLDEN_BUILD)/golden_gen
	./$(GOLDEN_BUILD)/golden_gen > $(GOLDEN_BUILD)/golden.c
	mv $(GOLDEN_BUILD)/golden.c bench/golden.c

# ---- bench firmware ----

# Two images, each flashed and read in turn: utils (flexutils, with newlib's
# printf and strtod baselines) and math (flexmath, with CMSIS-DSP and libm).
BENCH_SETS     = utils math
BENCH_SET     ?=
BENCH_OPT     ?= -O2
BENCH_BUILD    = $(BUILD)/bench$(BENCH_OPT)-$(BENCH_SET)
BENCH_PORT    ?= /dev/ttyACM0
BENCH_RESULTS ?= docs/results-f103$(filter-out -O2,$(BENCH_OPT)).md
FLEX_DIR      ?= bench/flex
CMSIS_DSP_DIR ?= bench/cmsis-dsp

ifneq ($(wildcard $(FLEX_DIR)/flex.mk),)
FLEX_MODULES := clock gpio usb
include $(FLEX_DIR)/flex.mk
endif

BENCH_CASES_utils = bench/cases_arch.c bench/cases_bits.c bench/cases_fmt.c \
                    bench/cases_crc.c bench/cases_ring.c bench/cases_parse.c \
                    bench/cases_cobs.c bench/cases_debounce.c bench/cases_rand.c
BENCH_CASES_math  = bench/cases_fixed.c bench/cases_sqrt.c bench/cases_div.c \
                    bench/cases_trig.c bench/cases_libm.c

BENCH_SRCS = bench/main.c bench/cache_probe.c bench/bench_core.c \
             $(BENCH_CASES_$(BENCH_SET)) bench/golden.c $(LIB_SRCS)
BENCH_DEFS = -DSTM32F103xB -DBENCH_OPT_NAME='"$(BENCH_OPT)"' \
             -DBENCH_SET_$(BENCH_SET) -DBENCH_SET_NAME='"$(BENCH_SET)"'

ifeq ($(BENCH_SET),math)
ifneq ($(wildcard $(CMSIS_DSP_DIR)/Include/arm_math.h),)
BENCH_SRCS    += bench/cases_cmsis.c
BENCH_DEFS    += -DBENCH_HAVE_CMSIS_DSP
CMSIS_DSP_INC  = -isystem $(CMSIS_DSP_DIR)/Include -isystem $(CMSIS_DSP_DIR)/PrivateInclude
CMSIS_DSP_SRCS = $(addprefix $(CMSIS_DSP_DIR)/Source/, \
                   FastMathFunctions/arm_sin_q15.c \
                   FastMathFunctions/arm_sin_q31.c \
                   FastMathFunctions/arm_sin_f32.c \
                   FastMathFunctions/arm_sqrt_q15.c \
                   FastMathFunctions/arm_sqrt_q31.c \
                   FastMathFunctions/arm_divide_q15.c \
                   FastMathFunctions/arm_divide_q31.c \
                   BasicMathFunctions/arm_abs_q15.c \
                   BasicMathFunctions/arm_abs_q31.c \
                   CommonTables/arm_common_tables.c)
endif
endif

BENCH_BASE_CFLAGS = $(M3_FLAGS) $(STD) $(BENCH_OPT) -g3 \
                    -ffunction-sections -fdata-sections $(BENCH_DEFS) \
                    $(LIB_INC) -Ibench -Iutils/src -Imath/src $(FLEX_INC) $(CMSIS_DSP_INC)
# _printf_float: newlib-nano's %f, for the fu_fmt_f32 baseline
BENCH_LDFLAGS = $(M3_FLAGS) -Tbench/linker.ld -Wl,--gc-sections \
                -Wl,--no-warn-rwx-segments --specs=nano.specs --specs=nosys.specs \
                $(if $(filter utils,$(BENCH_SET)),-u _printf_float) \
                -Wl,-Map=$(BENCH_BUILD)/bench.map

# flexlib + bench sources get the strict warnings; flex and CMSIS-DSP don't
BENCH_OBJS = $(patsubst %.c,$(BENCH_BUILD)/%.o,$(BENCH_SRCS))
FLEX_OBJS  = $(patsubst %.c,$(BENCH_BUILD)/flex/%.o,$(notdir $(FLEX_SRCS)))
CMSIS_OBJS = $(patsubst %.c,$(BENCH_BUILD)/cmsis-dsp/%.o,$(notdir $(CMSIS_DSP_SRCS)))
vpath %.c $(sort $(dir $(FLEX_SRCS) $(CMSIS_DSP_SRCS)))

# rebuild when the flags change, e.g. once bench/cmsis-dsp appears
ifneq ($(FLEX_ROOT),)
BENCH_STAMP = $(BENCH_BUILD)/cflags
ifneq ($(file <$(BENCH_STAMP)),$(BENCH_BASE_CFLAGS))
$(shell mkdir -p $(BENCH_BUILD))
$(file >$(BENCH_STAMP),$(BENCH_BASE_CFLAGS))
endif
$(BENCH_OBJS) $(FLEX_OBJS) $(CMSIS_OBJS): $(BENCH_STAMP)
endif

$(BENCH_OBJS): $(BENCH_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(ARM_CC) $(BENCH_BASE_CFLAGS) $(WARN) $(WERROR) -MMD -MP -c $< -o $@

$(FLEX_OBJS): $(BENCH_BUILD)/flex/%.o: %.c
	@mkdir -p $(dir $@)
	$(ARM_CC) $(BENCH_BASE_CFLAGS) -Wall -Wextra -MMD -MP -c $< -o $@

$(CMSIS_OBJS): $(BENCH_BUILD)/cmsis-dsp/%.o: %.c
	@mkdir -p $(dir $@)
	$(ARM_CC) $(BENCH_BASE_CFLAGS) -MMD -MP -c $< -o $@

$(BENCH_BUILD)/startup.o: $(FLEX_STARTUP)
	@mkdir -p $(dir $@)
	$(ARM_CC) $(M3_FLAGS) -c $< -o $@

$(BENCH_BUILD)/bench.elf: $(BENCH_OBJS) $(FLEX_OBJS) $(CMSIS_OBJS) $(BENCH_BUILD)/startup.o
	$(ARM_CC) $(BENCH_LDFLAGS) $^ -lm -o $@
	$(ARM_SIZE) $@

$(BENCH_BUILD)/bench.bin: $(BENCH_BUILD)/bench.elf
	$(ARM_OBJCOPY) -O binary $< $@

ifeq ($(FLEX_ROOT),)
bench bench-run bench-windows:
	@echo "$(FLEX_DIR) is missing: git submodule update --init, or FLEX_DIR=../flex"
	@exit 1
else ifeq ($(BENCH_SET),)
# each image in turn; bench-run writes the results file, then appends
bench bench-windows:
	@set -e; for s in $(BENCH_SETS); do \
		$(MAKE) --no-print-directory $@ BENCH_SET=$$s; done

bench-run:
	@set -e; a=; for s in $(BENCH_SETS); do \
		$(MAKE) --no-print-directory bench-run BENCH_SET=$$s BENCH_APPEND=$$a; \
		a=--append; done
else
bench: $(BENCH_BUILD)/bench.bin

# --reset: without it the old program's peripherals keep running
bench-run: $(BENCH_BUILD)/bench.bin
	st-flash --reset write $< 0x8000000
	$(PYTHON) tools/bench_run.py --port $(BENCH_PORT) --out $(BENCH_RESULTS) \
		$(BENCH_APPEND)

# what each case times: the instructions between its two CYCCNT reads
bench-windows: $(BENCH_BUILD)/bench.elf
	$(PYTHON) tools/bench_windows.py --objdump $(ARM_OBJDUMP) \
		$(filter $(BENCH_BUILD)/bench/%,$(BENCH_OBJS))
endif

# ---- generated sources ----

tables:
	$(PYTHON) tools/gen_crc_tables.py > utils/src/fu_crc_tab.c
	$(PYTHON) tools/gen_math_tables.py > math/src/fm_tables.c

# ---- misc ----

# compile_commands.json for clangd (needs bear)
compdb:
	bear -- $(MAKE) -B cross host-test

clean:
	rm -rf $(BUILD)

-include $(CROSS_OBJS:.o=.d) $(BENCH_OBJS:.o=.d) $(FLEX_OBJS:.o=.d) $(CMSIS_OBJS:.o=.d)

.PHONY: all check host-test exhaustive exhaustive-slow accuracy cross tables asm-audit size lint golden bench \
        bench-run bench-windows compdb clean
