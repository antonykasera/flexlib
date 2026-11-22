# host tests, and the exhaustive checks (every 32-bit input, on all cores)

HOST_BUILD  = $(BUILD)/host-$(HOST_CC)
HOST_CFLAGS = $(STD) $(WARN) $(WERROR) -O2 -g \
              -fsanitize=address,undefined -fno-sanitize-recover=undefined \
              -fno-omit-frame-pointer $(LIB_INC) $(LIB_PRIV) -Itest/host
TEST_SRCS   = $(wildcard test/host/*.c) $(LIB_SRCS)

HOST_RUNNERS           = all crc_small dsp fft4096 round_away round_gemmlowp round_tflite
HOST_SHARDS_all        = 0 1 2 3 4 5 6 7 8 9 10 11
HOST_SHARDS_dsp        = 0 1 2
HOST_DEFS_crc_small    = -DFU_CRC_SMALL
HOST_TESTS_crc_small   = fu_crc
HOST_DEFS_dsp          = -DFX_EMULATE_DSP
HOST_TESTS_dsp         = fd_
HOST_DEFS_fft4096      = -DFD_FFT_MAX=4096
HOST_TESTS_fft4096     = fd_fft
HOST_DEFS_round_away   = -DFNN_ROUNDING=FNN_ROUND_AWAY
HOST_TESTS_round_away  = fnn_
HOST_DEFS_round_gemmlowp  = -DFNN_ROUNDING=FNN_ROUND_GEMMLOWP
HOST_TESTS_round_gemmlowp = fnn_
HOST_DEFS_round_tflite    = -DFNN_ROUNDING=FNN_ROUND_TFLITE
HOST_TESTS_round_tflite   = fnn_

define HOST_RUNNER
$(HOST_BUILD)/$(1)/%.o: %.c
	@mkdir -p $$(@D)
	$$(HOST_CC) $$(HOST_CFLAGS) $$(HOST_DEFS_$(1)) -MMD -MP -c $$< -o $$@

$(HOST_BUILD)/$(1)/test_runner: $(patsubst %.c,$(HOST_BUILD)/$(1)/%.o,$(TEST_SRCS))
	$$(HOST_CC) $$(HOST_CFLAGS) $$^ -lm -o $$@

endef

define HOST_SHARD
host-test-$(1)-$(2): $(HOST_BUILD)/$(1)/test_runner
	@./$$< $$(HOST_TESTS_$(1)) --shard=$(2)/$(3) > $$<.$(2).log 2>&1 || \
		{ cat $$<.$(2).log; echo "--- $(1) $(2)/$(3) failed ($(HOST_CC))"; exit 1; }
	@echo "  $(1) $(2)/$(3): $$$$(tail -n 1 $$<.$(2).log)"

.PHONY: host-test-$(1)-$(2)
endef

host_shards = $(or $(HOST_SHARDS_$(1)),0)
$(foreach r,$(HOST_RUNNERS),$(eval $(call HOST_RUNNER,$(r))) \
  $(foreach i,$(call host_shards,$(r)), \
    $(eval $(call HOST_SHARD,$(r),$(i),$(words $(call host_shards,$(r)))))))

host-test: $(foreach r,$(HOST_RUNNERS),$(addprefix host-test-$(r)-,$(call host_shards,$(r))))
	@echo "--- Host tests passed ($(HOST_CC)) ---"

-include $(wildcard $(HOST_BUILD)/*/*/*/*.d)

# exhaustive-slow runs the *_slow.c checks (~13 min), kept out of CI
EXH_BUILD        = $(BUILD)/exhaustive
EXH_CFLAGS       = $(STD) $(WARN) $(WERROR) -O2 -fopenmp $(LIB_INC) $(LIB_PRIV)
EXHAUSTIVE_ALL   = $(wildcard test/exhaustive/*.c)
EXHAUSTIVE_SLOW  = $(filter %_slow.c,$(EXHAUSTIVE_ALL))
EXH_BUILDS       = plain fft4096 round_away round_gemmlowp
EXH_DEFS_fft4096        = -DFD_FFT_MAX=4096
EXH_DEFS_round_away     = -DFNN_ROUNDING=FNN_ROUND_AWAY
EXH_DEFS_round_gemmlowp = -DFNN_ROUNDING=FNN_ROUND_GEMMLOWP

define EXH_OBJECTS
$(EXH_BUILD)/$(1)/%.o: %.c
	@mkdir -p $$(@D)
	$$(HOST_CC) $$(EXH_CFLAGS) $$(EXH_DEFS_$(1)) -MMD -MP -c $$< -o $$@

$(EXH_BUILD)/$(1)/%: $(EXH_BUILD)/$(1)/test/exhaustive/%.o \
                     $(patsubst %.c,$(EXH_BUILD)/$(1)/%.o,$(LIB_SRCS))
	$$(HOST_CC) -fopenmp $$^ -lm -o $$@
endef
$(foreach b,$(EXH_BUILDS),$(eval $(call EXH_OBJECTS,$(b))))

exh_bins = $(patsubst test/exhaustive/%.c,$(EXH_BUILD)/plain/%,$(1))
EXH_FAST = $(call exh_bins,$(filter-out $(EXHAUSTIVE_SLOW),$(EXHAUSTIVE_ALL))) \
           $(EXH_BUILD)/fft4096/exhaustive_fd_fft \
           $(EXH_BUILD)/round_away/exhaustive_fnn_quant \
           $(EXH_BUILD)/round_gemmlowp/exhaustive_fnn_quant \
           $(EXH_BUILD)/round_away/exhaustive_fnn_convert \
           $(EXH_BUILD)/round_gemmlowp/exhaustive_fnn_convert

exhaustive: $(EXH_FAST)
	@set -e; for b in $^; do echo "--- $$b"; ./$$b; done

exhaustive-slow: $(call exh_bins,$(EXHAUSTIVE_SLOW))
	@set -e; for b in $^; do echo "--- $$b"; ./$$b; done

.PRECIOUS: $(EXH_BUILD)/%.o
-include $(wildcard $(EXH_BUILD)/*/*/*/*.d)

.PHONY: host-test exhaustive exhaustive-slow
