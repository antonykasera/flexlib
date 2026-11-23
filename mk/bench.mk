# golden hashes (the bench cases run on the host) and the bench firmware

FLEX_DIR      ?= third_party/flex
CMSIS_DSP_DIR ?= third_party/cmsis-dsp
CMSIS_NN_DIR  ?= third_party/cmsis-nn

BENCH_INC = -Ibench/core -Ibench/models $(LIB_PRIV)

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
BENCH_CASES_dsp5   = bench/cases/cases_fd_resample.c
BENCH_CASES_dsp6   = bench/cases/cases_fd_f32.c
BENCH_CASES_nn     = bench/cases/cases_fnn.c
BENCH_CASES_nn2    = bench/cases/cases_fnn2.c $(wildcard bench/models/*.c)
# everything that also builds on the host (golden, lint)
BENCH_PORTABLE = bench/core/bench_core.c $(BENCH_CASES_utils) \
                 $(BENCH_CASES_math) $(BENCH_CASES_struct) $(BENCH_CASES_dsp) \
                 $(BENCH_CASES_dsp2) $(BENCH_CASES_dsp3) $(BENCH_CASES_dsp4) \
                 $(BENCH_CASES_dsp5) $(BENCH_CASES_dsp6) $(BENCH_CASES_nn) \
                 $(BENCH_CASES_nn2)

GOLDEN_BUILD        = $(BUILD)/golden
GOLDEN_CFLAGS       = $(STD) $(WARN) $(WERROR) -O2 -DBENCH_HOST $(LIB_INC) $(BENCH_INC)
GOLDEN_SRCS         = bench/core/golden_gen.c $(BENCH_PORTABLE) $(LIB_SRCS)
GOLDEN_BUILDS       = plain dsp fpu fpu_dsp
GOLDEN_DEFS_dsp     = -DFX_EMULATE_DSP
GOLDEN_DEFS_fpu     = -DFX_EMULATE_FPU
GOLDEN_DEFS_fpu_dsp = -DFX_EMULATE_FPU -DFX_EMULATE_DSP

define GOLDEN_GEN
$(GOLDEN_BUILD)/$(1)/%.o: %.c
	@mkdir -p $$(@D)
	$$(HOST_CC) $$(GOLDEN_CFLAGS) $$(GOLDEN_DEFS_$(1)) -MMD -MP -c $$< -o $$@

$(GOLDEN_BUILD)/$(1)/golden_gen: $(patsubst %.c,$(GOLDEN_BUILD)/$(1)/%.o,$(GOLDEN_SRCS))
	$$(HOST_CC) $$^ -lm -o $$@
endef
$(foreach g,$(GOLDEN_BUILDS),$(eval $(call GOLDEN_GEN,$(g))))

golden: $(foreach g,$(GOLDEN_BUILDS),$(GOLDEN_BUILD)/$(g)/golden_gen)
	$(GOLDEN_BUILD)/plain/golden_gen > $(GOLDEN_BUILD)/golden.c
	mv $(GOLDEN_BUILD)/golden.c bench/core/golden.c
	$(GOLDEN_BUILD)/dsp/golden_gen | cmp - bench/core/golden.c
	$(GOLDEN_BUILD)/fpu/golden_gen > $(GOLDEN_BUILD)/golden_fpu.c
	mv $(GOLDEN_BUILD)/golden_fpu.c bench/core/golden_fpu.c
	$(GOLDEN_BUILD)/fpu_dsp/golden_gen | cmp - bench/core/golden_fpu.c

-include $(wildcard $(GOLDEN_BUILD)/*/*/*/*.d)

BENCH_SETS     = utils math struct dsp dsp2 dsp3 dsp4 dsp5 dsp6 nn nn2
BENCH_SET     ?=
BENCH_OPT     ?= -O2
# bench/port/<target>/port.mk: CPU, report, OpenOCD target
BENCH_TARGET  ?= f103
BENCH_PORTDIR  = bench/port/$(BENCH_TARGET)
include $(BENCH_PORTDIR)/port.mk
# usb: CDC on BENCH_PORT; rtt: SEGGER RTT over SWD (OpenOCD)
BENCH_BUILD    = $(BUILD)/bench$(filter-out -f103,-$(BENCH_TARGET))$(BENCH_OPT)-$(BENCH_SET)$(filter-out -usb,-$(BENCH_REPORT))
BENCH_PORT    ?= /dev/ttyACM0
ARM_NM        ?= arm-none-eabi-nm
BENCH_RESULTS ?= docs/results/$(BENCH_TARGET)$(filter-out -O2,$(BENCH_OPT)).md

BENCH_REPORT_SRC = $(if $(filter rtt,$(BENCH_REPORT)),bench/core/report_rtt.c,$(BENCH_PORTDIR)/report_$(BENCH_REPORT).c)
BENCH_SRCS = bench/core/bench_main.c $(BENCH_REPORT_SRC) $(BENCH_PORT_SRCS) \
             bench/core/bench_core.c $(BENCH_CASES_$(BENCH_SET)) \
             $(BENCH_GOLDEN) $(LIB_SRCS)
BENCH_DEFS = $(BENCH_PORT_DEFS) -DBENCH_OPT_NAME='"$(BENCH_OPT)"' \
             -DBENCH_SET_$(BENCH_SET) -DBENCH_SET_NAME='"$(BENCH_SET)"'

ifneq ($(filter math struct dsp dsp2 dsp3 dsp4 dsp5 dsp6,$(BENCH_SET)),)
ifneq ($(wildcard $(CMSIS_DSP_DIR)/Include/arm_math.h),)
BENCH_SRCS    += $(if $(filter math,$(BENCH_SET)),bench/cases/cases_cmsis.c)
BENCH_DEFS    += -DBENCH_HAVE_CMSIS_DSP
CMSIS_DSP_INC  = -isystem $(CMSIS_DSP_DIR)/Include -isystem $(CMSIS_DSP_DIR)/PrivateInclude
CMSIS_DSP_SRCS = $(addprefix $(CMSIS_DSP_DIR)/Source/,$(CMSIS_SRCS_$(BENCH_SET)))
# CMSIS-DSP's own build unrolls its loops, and the dsp images' baselines keep that
CMSIS_DSP_DEFS = $(if $(filter dsp dsp2 dsp3 dsp4 dsp5 dsp6,$(BENCH_SET)),-DARM_MATH_LOOPUNROLL)
endif
endif

# CMSIS-NN as it ships, with no options and its gemmlowp rounding
ifneq ($(filter nn nn2,$(BENCH_SET)),)
ifneq ($(wildcard $(CMSIS_NN_DIR)/Include/arm_nnfunctions.h),)
BENCH_DEFS    += -DBENCH_HAVE_CMSIS_NN
CMSIS_NN_INC   = -isystem $(CMSIS_NN_DIR)/Include
CMSIS_NN_SRCS  = $(addprefix $(CMSIS_NN_DIR)/Source/,$(CMSIS_NN_SRCS_$(BENCH_SET)))
endif
endif

CMSIS_NN_SRCS_nn  = $(addprefix FullyConnectedFunctions/,arm_fully_connected_s8.c \
                      arm_fully_connected_get_buffer_sizes_s8.c) \
                    $(addprefix ConvolutionFunctions/,arm_convolve_1x1_s8_fast.c \
                      arm_convolve_s8.c arm_convolve_1_x_n_s8.c \
                      arm_convolve_get_buffer_sizes_s8.c arm_depthwise_conv_s8.c \
                      arm_depthwise_conv_s8_opt.c arm_depthwise_conv_get_buffer_sizes_s8.c \
                      arm_nn_mat_mult_kernel_s8_s16.c \
                      arm_nn_mat_mult_kernel_row_offset_s8_s16.c \
                      arm_nn_depthwise_conv_s8_core.c) \
                    $(addprefix PoolingFunctions/,arm_max_pool_s8.c arm_avgpool_s8.c \
                      arm_avgpool_get_buffer_sizes_s8.c) \
                    BasicMathFunctions/arm_elementwise_add_s8.c \
                    ConcatenationFunctions/arm_concatenation_s8_x.c \
                    $(addprefix SoftmaxFunctions/,arm_softmax_s8.c \
                      arm_nn_softmax_common_s8.c) \
                    $(addprefix NNSupportFunctions/,arm_nn_mat_mul_core_1x_s8.c \
                      arm_nn_mat_mul_core_4x_s8.c arm_nn_mat_mult_nt_t_s8.c \
                      arm_nn_vec_mat_mult_t_s8.c arm_nn_vec_mat_mult_t_per_ch_s8.c \
                      arm_nn_depthwise_conv_nt_t_s8.c \
                      arm_nn_depthwise_conv_nt_t_padded_s8.c \
                      arm_q7_to_q15_with_offset.c arm_s8_to_s16_unordered_with_offset.c \
                      arm_nntables.c)

CMSIS_NN_SRCS_nn2 = FullyConnectedFunctions/arm_fully_connected_s8.c \
                    $(addprefix ConvolutionFunctions/,arm_convolve_wrapper_s8.c \
                      arm_convolve_s8.c arm_convolve_1_x_n_s8.c arm_convolve_1x1_s8.c \
                      arm_convolve_1x1_s8_fast.c arm_convolve_get_buffer_sizes_s8.c \
                      arm_nn_mat_mult_kernel_s8_s16.c \
                      arm_nn_mat_mult_kernel_row_offset_s8_s16.c) \
                    $(addprefix PoolingFunctions/,arm_max_pool_s8.c arm_avgpool_s8.c \
                      arm_avgpool_get_buffer_sizes_s8.c) \
                    $(addprefix SoftmaxFunctions/,arm_softmax_s8.c \
                      arm_nn_softmax_common_s8.c) \
                    $(addprefix NNSupportFunctions/,arm_nn_mat_mul_core_1x_s8.c \
                      arm_nn_mat_mul_core_4x_s8.c arm_nn_mat_mult_nt_t_s8.c \
                      arm_nn_vec_mat_mult_t_s8.c arm_q7_to_q15_with_offset.c \
                      arm_s8_to_s16_unordered_with_offset.c arm_nntables.c)

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
CMSIS_SRCS_dsp5   = $(addprefix FilteringFunctions/,arm_fir_interpolate_q15.c \
                      arm_fir_interpolate_init_q15.c arm_fir_interpolate_q31.c \
                      arm_fir_interpolate_init_q31.c)
CMSIS_SRCS_dsp6   = $(addprefix FilteringFunctions/,arm_fir_f32.c arm_fir_init_f32.c \
                      arm_biquad_cascade_df2T_f32.c arm_biquad_cascade_df2T_init_f32.c \
                      arm_conv_f32.c arm_correlate_f32.c) \
                    $(addprefix TransformFunctions/,arm_cfft_f32.c arm_cfft_radix8_f32.c \
                      arm_cfft_init_f32.c arm_rfft_fast_f32.c arm_rfft_fast_init_f32.c \
                      arm_bitreversal2.c) \
                    $(addprefix BasicMathFunctions/,arm_dot_prod_f32.c arm_add_f32.c \
                      arm_mult_f32.c arm_abs_f32.c) \
                    $(addprefix StatisticsFunctions/,arm_power_f32.c arm_mean_f32.c \
                      arm_rms_f32.c) \
                    CommonTables/arm_common_tables.c CommonTables/arm_const_structs.c

BENCH_BASE_CFLAGS = $(BENCH_CPU) $(STD) $(BENCH_OPT) -g3 \
                    -ffunction-sections -fdata-sections $(BENCH_DEFS) \
                    $(LIB_INC) $(BENCH_INC) -I$(BENCH_PORTDIR) $(BENCH_PORT_INC) \
                    $(CMSIS_DSP_INC) $(CMSIS_NN_INC)
# _printf_float: newlib-nano's %f, for the fu_fmt_f32 baseline
BENCH_LDFLAGS = $(BENCH_CPU) -T$(BENCH_PORTDIR)/linker.ld -Wl,--gc-sections \
                -Wl,--no-warn-rwx-segments --specs=nano.specs --specs=nosys.specs \
                $(if $(filter utils,$(BENCH_SET)),-u _printf_float) \
                -Wl,-Map=$(BENCH_BUILD)/bench.map

# strict warnings for flexlib and the bench, not for the board's sources
BENCH_OBJS = $(patsubst %.c,$(BENCH_BUILD)/%.o,$(BENCH_SRCS))
EXT_OBJS   = $(patsubst %.c,$(BENCH_BUILD)/ext/%.o,$(notdir $(BENCH_EXT_SRCS)))
CMSIS_OBJS = $(patsubst %.c,$(BENCH_BUILD)/cmsis-dsp/%.o,$(notdir $(CMSIS_DSP_SRCS))) \
             $(patsubst %.c,$(BENCH_BUILD)/cmsis-nn/%.o,$(notdir $(CMSIS_NN_SRCS)))
vpath %.c $(sort $(dir $(BENCH_EXT_SRCS) $(CMSIS_DSP_SRCS) $(CMSIS_NN_SRCS)))

# rebuild when the flags change
ifeq ($(BENCH_MISSING),)
BENCH_STAMP = $(BENCH_BUILD)/cflags
ifneq ($(file <$(BENCH_STAMP)),$(BENCH_BASE_CFLAGS))
$(shell mkdir -p $(BENCH_BUILD))
$(file >$(BENCH_STAMP),$(BENCH_BASE_CFLAGS))
endif
$(BENCH_OBJS) $(EXT_OBJS) $(CMSIS_OBJS): $(BENCH_STAMP)
endif

$(BENCH_OBJS): $(BENCH_BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(ARM_CC) $(BENCH_BASE_CFLAGS) $(WARN) $(WERROR) -MMD -MP -c $< -o $@

$(EXT_OBJS): $(BENCH_BUILD)/ext/%.o: %.c
	@mkdir -p $(dir $@)
	$(ARM_CC) $(BENCH_BASE_CFLAGS) -Wall -Wextra -MMD -MP -c $< -o $@

$(filter $(BENCH_BUILD)/cmsis-dsp/%,$(CMSIS_OBJS)): $(BENCH_BUILD)/cmsis-dsp/%.o: %.c
	@mkdir -p $(dir $@)
	$(ARM_CC) $(BENCH_BASE_CFLAGS) $(CMSIS_DSP_DEFS) -MMD -MP -c $< -o $@

$(filter $(BENCH_BUILD)/cmsis-nn/%,$(CMSIS_OBJS)): $(BENCH_BUILD)/cmsis-nn/%.o: %.c
	@mkdir -p $(dir $@)
	$(ARM_CC) $(BENCH_BASE_CFLAGS) -MMD -MP -c $< -o $@

$(BENCH_BUILD)/startup.o: $(BENCH_STARTUP)
	@mkdir -p $(dir $@)
	$(ARM_CC) $(BENCH_CPU) -c $< -o $@

$(BENCH_BUILD)/bench.elf: $(BENCH_OBJS) $(EXT_OBJS) $(CMSIS_OBJS) $(BENCH_BUILD)/startup.o \
                          $(BENCH_PORTDIR)/linker.ld
	$(ARM_CC) $(BENCH_LDFLAGS) $(filter %.o,$^) -lm -o $@
	$(ARM_SIZE) $@

$(BENCH_BUILD)/bench.bin: $(BENCH_BUILD)/bench.elf
	$(ARM_OBJCOPY) -O binary $< $@

ifneq ($(BENCH_MISSING),)
bench bench-run bench-windows:
	@echo "$(BENCH_MISSING)"
	@exit 1
else ifeq ($(BENCH_SET),)
bench: $(addprefix bench-image-,$(BENCH_SETS))

$(addprefix bench-image-,$(BENCH_SETS)): bench-image-%:
	@$(MAKE) --no-print-directory bench BENCH_SET=$*

bench-windows:
	@set -e; for s in $(BENCH_SETS); do \
		$(MAKE) --no-print-directory $@ BENCH_SET=$$s; done

bench-run:
	@set -e; a=; for s in $(BENCH_SETS); do \
		$(MAKE) --no-print-directory bench-run BENCH_SET=$$s BENCH_APPEND=$$a; \
		a=--append; done
else
bench: $(BENCH_BUILD)/bench.bin

ifeq ($(BENCH_REPORT),rtt)
bench-run: $(BENCH_BUILD)/bench.elf
	$(PYTHON) tools/bench_run.py --rtt $< --openocd "$(BENCH_OPENOCD)" \
		--nm $(ARM_NM) --title "$(BENCH_TITLE)" --out $(BENCH_RESULTS) $(BENCH_APPEND)
else
# --reset: without it the old program's peripherals keep running
bench-run: $(BENCH_BUILD)/bench.bin
	st-flash --reset write $< 0x8000000
	$(PYTHON) tools/bench_run.py --port $(BENCH_PORT) --title "$(BENCH_TITLE)" \
		--out $(BENCH_RESULTS) $(BENCH_APPEND)
endif

# what each case times: the instructions between its two CYCCNT reads
bench-windows: $(BENCH_BUILD)/bench.elf
	$(PYTHON) tools/bench_windows.py --objdump $(ARM_OBJDUMP) \
		$(filter $(BENCH_BUILD)/bench/%,$(BENCH_OBJS))
endif

-include $(BENCH_OBJS:.o=.d) $(EXT_OBJS:.o=.d) $(CMSIS_OBJS:.o=.d)

.PHONY: golden bench bench-run bench-windows $(addprefix bench-image-,$(BENCH_SETS))
