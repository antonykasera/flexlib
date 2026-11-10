# flexmath, flexdsp and the CMSIS-DSP functions they replace, against references

FLEX_DIR      ?= third_party/flex
CMSIS_DSP_DIR ?= third_party/cmsis-dsp

ACC_BUILD = $(BUILD)/accuracy
ACC_CMSIS = $(addprefix $(CMSIS_DSP_DIR)/Source/, \
              BasicMathFunctions/arm_mult_q15.c \
              BasicMathFunctions/arm_mult_q31.c \
              SupportFunctions/arm_float_to_q15.c \
              SupportFunctions/arm_q31_to_float.c \
              FastMathFunctions/arm_sqrt_q15.c \
              FastMathFunctions/arm_sqrt_q31.c \
              FastMathFunctions/arm_sin_q15.c \
              FastMathFunctions/arm_sin_q31.c \
              FastMathFunctions/arm_sin_f32.c \
              FastMathFunctions/arm_cos_f32.c \
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
              BasicMathFunctions/arm_abs_q15.c \
              BasicMathFunctions/arm_abs_q31.c \
              BasicMathFunctions/arm_scale_q15.c \
              BasicMathFunctions/arm_dot_prod_q31.c \
              SupportFunctions/arm_q31_to_q15.c \
              StatisticsFunctions/arm_power_q31.c \
              StatisticsFunctions/arm_rms_q15.c \
              StatisticsFunctions/arm_var_q15.c \
              StatisticsFunctions/arm_std_q15.c \
              ComplexMathFunctions/arm_cmplx_mag_q15.c \
              ComplexMathFunctions/arm_cmplx_mult_cmplx_q15.c \
              FilteringFunctions/arm_fir_q15.c \
              FilteringFunctions/arm_fir_fast_q15.c \
              FilteringFunctions/arm_fir_init_q15.c \
              FilteringFunctions/arm_fir_q31.c \
              FilteringFunctions/arm_fir_fast_q31.c \
              FilteringFunctions/arm_fir_init_q31.c \
              FilteringFunctions/arm_biquad_cascade_df1_q15.c \
              FilteringFunctions/arm_biquad_cascade_df1_init_q15.c \
              FilteringFunctions/arm_biquad_cascade_df1_q31.c \
              FilteringFunctions/arm_biquad_cascade_df1_fast_q31.c \
              FilteringFunctions/arm_biquad_cascade_df1_init_q31.c \
              FilteringFunctions/arm_biquad_cascade_df1_32x64_q31.c \
              FilteringFunctions/arm_biquad_cascade_df1_32x64_init_q31.c \
              CommonTables/arm_common_tables.c)
ACC_CMSIS_INC = -isystem $(CMSIS_DSP_DIR)/Include \
                -isystem $(CMSIS_DSP_DIR)/PrivateInclude -isystem $(FLEX_DIR)/cmsis/inc

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

.PHONY: accuracy
