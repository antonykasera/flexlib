# golden hashes (the bench cases run on the host) and the bench firmware

FLEX_DIR      ?= third_party/flex
CMSIS_DSP_DIR ?= third_party/cmsis-dsp

BENCH_INC = -Ibench/core $(LIB_PRIV)

BENCH_CASES_utils  = $(addprefix bench/cases/,cases_arch.c cases_bits.c \
                       cases_fmt.c cases_crc.c cases_ring.c cases_parse.c \
                       cases_cobs.c cases_debounce.c cases_rand.c)
BENCH_CASES_math   = $(addprefix bench/cases/,cases_fixed.c cases_sqrt.c \
                       cases_div.c cases_trig.c cases_atan.c cases_exp.c \
                       cases_float.c cases_libm.c)
BENCH_CASES_struct = $(addprefix bench/cases/,cases_linalg.c cases_quat.c \
                       cases_pid.c)
BENCH_CASES_dsp    = $(addprefix bench/cases/,cases_pairs.c cases_fd_vec.c \
                       cases_fd_fir.c)
BENCH_CASES_dsp2   = $(addprefix bench/cases/,cases_fd_biquad.c cases_fd_tone.c)
BENCH_CASES_dsp3   = bench/cases/cases_fd_fft.c
BENCH_CASES_dsp4   = bench/cases/cases_fd_corr.c
# everything that also builds on the host (golden, lint)
BENCH_PORTABLE = bench/core/bench_core.c $(BENCH_CASES_utils) \
                 $(BENCH_CASES_math) $(BENCH_CASES_struct) $(BENCH_CASES_dsp) \
                 $(BENCH_CASES_dsp2) $(BENCH_CASES_dsp3) $(BENCH_CASES_dsp4)

GOLDEN_BUILD = $(BUILD)/golden

golden:
	@mkdir -p $(GOLDEN_BUILD)
	$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -DBENCH_HOST $(LIB_INC) $(BENCH_INC) \
		bench/core/golden_gen.c $(BENCH_PORTABLE) $(LIB_SRCS) -lm \
		-o $(GOLDEN_BUILD)/golden_gen
	./$(GOLDEN_BUILD)/golden_gen > $(GOLDEN_BUILD)/golden.c
	mv $(GOLDEN_BUILD)/golden.c bench/core/golden.c
	$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -DBENCH_HOST -DFX_EMULATE_DSP $(LIB_INC) \
		$(BENCH_INC) bench/core/golden_gen.c $(BENCH_PORTABLE) $(LIB_SRCS) -lm \
		-o $(GOLDEN_BUILD)/golden_gen_dsp
	./$(GOLDEN_BUILD)/golden_gen_dsp | cmp - bench/core/golden.c

BENCH_SETS     = utils math struct dsp dsp2 dsp3 dsp4
BENCH_SET     ?=
BENCH_OPT     ?= -O2
BENCH_BUILD    = $(BUILD)/bench$(BENCH_OPT)-$(BENCH_SET)
BENCH_PORT    ?= /dev/ttyACM0
BENCH_RESULTS ?= docs/results/f103$(filter-out -O2,$(BENCH_OPT)).md
BENCH_PORTDIR  = bench/port/f103

ifneq ($(wildcard $(FLEX_DIR)/flex.mk),)
FLEX_MODULES := clock gpio usb
include $(FLEX_DIR)/flex.mk
endif

BENCH_SRCS = $(BENCH_PORTDIR)/main.c $(BENCH_PORTDIR)/cache_probe.c \
             bench/core/bench_core.c $(BENCH_CASES_$(BENCH_SET)) \
             bench/core/golden.c $(LIB_SRCS)
BENCH_DEFS = -DSTM32F103xB -DBENCH_OPT_NAME='"$(BENCH_OPT)"' \
             -DBENCH_SET_$(BENCH_SET) -DBENCH_SET_NAME='"$(BENCH_SET)"'

ifneq ($(filter math struct dsp dsp2 dsp3 dsp4,$(BENCH_SET)),)
ifneq ($(wildcard $(CMSIS_DSP_DIR)/Include/arm_math.h),)
BENCH_SRCS    += $(if $(filter math,$(BENCH_SET)),bench/cases/cases_cmsis.c)
BENCH_DEFS    += -DBENCH_HAVE_CMSIS_DSP
CMSIS_DSP_INC  = -isystem $(CMSIS_DSP_DIR)/Include -isystem $(CMSIS_DSP_DIR)/PrivateInclude
CMSIS_DSP_SRCS = $(addprefix $(CMSIS_DSP_DIR)/Source/,$(CMSIS_SRCS_$(BENCH_SET)))
# the dsp image's baselines unroll their loops, as CMSIS-DSP's own build does
CMSIS_DSP_DEFS = $(if $(filter dsp dsp2 dsp3 dsp4,$(BENCH_SET)),-DARM_MATH_LOOPUNROLL)
endif
endif

CMSIS_SRCS_math   = FastMathFunctions/arm_sin_q15.c \
                    FastMathFunctions/arm_sin_q31.c \
                    FastMathFunctions/arm_sin_f32.c \
                    FastMathFunctions/arm_cos_f32.c \
                    FastMathFunctions/arm_sqrt_q15.c \
                    FastMathFunctions/arm_sqrt_q31.c \
                    FastMathFunctions/arm_divide_q15.c \
                    FastMathFunctions/arm_divide_q31.c \
                    FastMathFunctions/arm_atan2_q15.c \
                    FastMathFunctions/arm_atan2_q31.c \
                    FastMathFunctions/arm_atan2_f32.c \
                    FastMathFunctions/arm_vlog_q15.c \
                    FastMathFunctions/arm_vlog_q31.c \
                    MatrixFunctions/arm_mat_mult_q31.c \
                    MatrixFunctions/arm_mat_mult_f32.c \
                    QuaternionMathFunctions/arm_quaternion_product_single_f32.c \
                    QuaternionMathFunctions/arm_quaternion_normalize_f32.c \
                    ControllerFunctions/arm_pid_init_q31.c \
                    ControllerFunctions/arm_pid_init_f32.c \
                    BasicMathFunctions/arm_abs_q15.c \
                    BasicMathFunctions/arm_abs_q31.c \
                    CommonTables/arm_common_tables.c
CMSIS_SRCS_struct = $(CMSIS_SRCS_math)
CMSIS_SRCS_dsp    = $(addprefix BasicMathFunctions/,arm_add_q15.c arm_add_q31.c \
                      arm_mult_q15.c arm_mult_q31.c arm_scale_q15.c \
                      arm_scale_q31.c arm_shift_q15.c arm_offset_q15.c \
                      arm_negate_q15.c arm_abs_q15.c arm_clip_q15.c \
                      arm_dot_prod_q15.c arm_dot_prod_q31.c) \
                    $(addprefix SupportFunctions/,arm_fill_q15.c arm_copy_q15.c \
                      arm_q31_to_q15.c arm_q15_to_float.c arm_float_to_q15.c) \
                    $(addprefix StatisticsFunctions/,arm_mean_q15.c \
                      arm_power_q15.c arm_power_q31.c arm_rms_q15.c \
                      arm_rms_q31.c arm_var_q15.c arm_std_q15.c arm_max_q15.c \
                      arm_absmax_q15.c) \
                    $(addprefix ComplexMathFunctions/,arm_cmplx_mult_cmplx_q15.c \
                      arm_cmplx_mult_cmplx_q31.c arm_cmplx_mag_q15.c \
                      arm_cmplx_mag_q31.c arm_cmplx_mag_squared_q15.c) \
                    $(addprefix FilteringFunctions/,arm_fir_q15.c arm_fir_fast_q15.c \
                      arm_fir_init_q15.c arm_fir_q31.c arm_fir_fast_q31.c \
                      arm_fir_init_q31.c arm_fir_decimate_q15.c \
                      arm_fir_decimate_init_q15.c arm_fir_interpolate_q15.c \
                      arm_fir_interpolate_init_q15.c) \
                    FastMathFunctions/arm_sqrt_q15.c FastMathFunctions/arm_sqrt_q31.c \
                    CommonTables/arm_common_tables.c
CMSIS_SRCS_dsp2   = $(addprefix FilteringFunctions/,arm_biquad_cascade_df1_q15.c \
                      arm_biquad_cascade_df1_init_q15.c arm_biquad_cascade_df1_fast_q15.c \
                      arm_biquad_cascade_df1_q31.c arm_biquad_cascade_df1_init_q31.c \
                      arm_biquad_cascade_df1_fast_q31.c arm_biquad_cascade_df1_32x64_q31.c \
                      arm_biquad_cascade_df1_32x64_init_q31.c) \
                    WindowFunctions/arm_hanning_f32.c CommonTables/arm_common_tables.c
CMSIS_SRCS_dsp3   = $(addprefix TransformFunctions/,arm_cfft_q15.c arm_cfft_q31.c \
                      arm_cfft_radix4_q15.c arm_cfft_radix4_q31.c \
                      arm_bitreversal2.c arm_cfft_init_q15.c arm_rfft_q15.c \
                      arm_rfft_init_q15.c) \
                    BasicMathFunctions/arm_shift_q15.c \
                    CommonTables/arm_common_tables.c CommonTables/arm_const_structs.c
CMSIS_SRCS_dsp4   = $(addprefix FilteringFunctions/,arm_conv_q15.c arm_conv_fast_q15.c \
                      arm_conv_q31.c arm_correlate_q15.c arm_correlate_q31.c \
                      arm_lms_q15.c arm_lms_init_q15.c arm_lms_norm_q15.c \
                      arm_lms_norm_init_q15.c arm_lms_q31.c arm_lms_init_q31.c \
                      arm_levinson_durbin_q31.c) \
                    FastMathFunctions/arm_divide_q15.c BasicMathFunctions/arm_abs_q15.c \
                    CommonTables/arm_common_tables.c

BENCH_BASE_CFLAGS = $(M3_FLAGS) $(STD) $(BENCH_OPT) -g3 \
                    -ffunction-sections -fdata-sections $(BENCH_DEFS) \
                    $(LIB_INC) $(BENCH_INC) $(FLEX_INC) $(CMSIS_DSP_INC)
# _printf_float: newlib-nano's %f, for the fu_fmt_f32 baseline
BENCH_LDFLAGS = $(M3_FLAGS) -T$(BENCH_PORTDIR)/linker.ld -Wl,--gc-sections \
                -Wl,--no-warn-rwx-segments --specs=nano.specs --specs=nosys.specs \
                $(if $(filter utils,$(BENCH_SET)),-u _printf_float) \
                -Wl,-Map=$(BENCH_BUILD)/bench.map

# flexlib + bench sources get the strict warnings
BENCH_OBJS = $(patsubst %.c,$(BENCH_BUILD)/%.o,$(BENCH_SRCS))
FLEX_OBJS  = $(patsubst %.c,$(BENCH_BUILD)/flex/%.o,$(notdir $(FLEX_SRCS)))
CMSIS_OBJS = $(patsubst %.c,$(BENCH_BUILD)/cmsis-dsp/%.o,$(notdir $(CMSIS_DSP_SRCS)))
vpath %.c $(sort $(dir $(FLEX_SRCS) $(CMSIS_DSP_SRCS)))

# rebuild when the flags change
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
	$(ARM_CC) $(BENCH_BASE_CFLAGS) $(CMSIS_DSP_DEFS) -MMD -MP -c $< -o $@

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
	@echo "$(FLEX_DIR) is missing: make deps, or FLEX_DIR=../flex"
	@exit 1
else ifeq ($(BENCH_SET),)
# each image in turn
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

-include $(BENCH_OBJS:.o=.d) $(FLEX_OBJS:.o=.d) $(CMSIS_OBJS:.o=.d)

.PHONY: golden bench bench-run bench-windows
