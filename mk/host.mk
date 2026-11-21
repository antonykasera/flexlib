# host tests, and the exhaustive checks (every 32-bit input, on all cores)

HOST_BUILD  = $(BUILD)/host-$(HOST_CC)
HOST_CFLAGS = $(STD) $(WARN) $(WERROR) -O2 -g \
              -fsanitize=address,undefined -fno-sanitize-recover=undefined \
              -fno-omit-frame-pointer $(LIB_INC) $(LIB_PRIV) -Itest/host
TEST_SRCS   = $(wildcard test/host/*.c) $(LIB_SRCS)

host-test:
	@mkdir -p $(HOST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_SRCS) -lm -o $(HOST_BUILD)/test_runner
	./$(HOST_BUILD)/test_runner
	$(HOST_CC) $(HOST_CFLAGS) -DFU_CRC_SMALL $(TEST_SRCS) -lm \
		-o $(HOST_BUILD)/test_runner_crc_small
	./$(HOST_BUILD)/test_runner_crc_small fu_crc
	$(HOST_CC) $(HOST_CFLAGS) -DFX_EMULATE_DSP $(TEST_SRCS) -lm \
		-o $(HOST_BUILD)/test_runner_dsp
	./$(HOST_BUILD)/test_runner_dsp fd_
	$(HOST_CC) $(HOST_CFLAGS) -DFD_FFT_MAX=4096 $(TEST_SRCS) -lm \
		-o $(HOST_BUILD)/test_runner_fft4096
	./$(HOST_BUILD)/test_runner_fft4096 fd_fft
	@set -e; for m in 1 2; do \
		$(HOST_CC) $(HOST_CFLAGS) -DFNN_ROUNDING=$$m $(TEST_SRCS) -lm \
			-o $(HOST_BUILD)/test_runner_round$$m; \
		./$(HOST_BUILD)/test_runner_round$$m fnn_; \
	done
	@echo "--- Host tests passed ($(HOST_CC)) ---"

# exhaustive-slow runs the *_slow.c checks (~13 min), kept out of CI
EXHAUSTIVE_ALL  = $(wildcard test/exhaustive/*.c)
EXHAUSTIVE_SLOW = $(filter %_slow.c,$(EXHAUSTIVE_ALL))

define RUN_EXHAUSTIVE
	@mkdir -p $(BUILD)/exhaustive
	@set -e; for t in $(1); do \
		b=$(BUILD)/exhaustive/$$(basename $$t .c); \
		$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -fopenmp $(LIB_INC) $(LIB_PRIV) $$t \
			$(LIB_SRCS) -lm -o $$b; \
		echo "--- $$t"; ./$$b; \
	done
endef

# the FFT again with its generated 4096-point table, requantisation in the
# other two rounding modes
exhaustive:
	$(call RUN_EXHAUSTIVE,$(filter-out $(EXHAUSTIVE_SLOW),$(EXHAUSTIVE_ALL)))
	$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -fopenmp -DFD_FFT_MAX=4096 $(LIB_INC) \
		test/exhaustive/exhaustive_fd_fft.c $(LIB_SRCS) -lm \
		-o $(BUILD)/exhaustive/exhaustive_fd_fft_4096
	@echo "--- test/exhaustive/exhaustive_fd_fft.c, FD_FFT_MAX=4096"
	./$(BUILD)/exhaustive/exhaustive_fd_fft_4096
	@set -e; for m in 1 2; do \
		$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -fopenmp -DFNN_ROUNDING=$$m $(LIB_INC) \
			test/exhaustive/exhaustive_fnn_quant.c $(LIB_SRCS) -lm \
			-o $(BUILD)/exhaustive/exhaustive_fnn_quant_$$m; \
		echo "--- test/exhaustive/exhaustive_fnn_quant.c, FNN_ROUNDING=$$m"; \
		./$(BUILD)/exhaustive/exhaustive_fnn_quant_$$m; \
	done

exhaustive-slow:
	$(call RUN_EXHAUSTIVE,$(EXHAUSTIVE_SLOW))

.PHONY: host-test exhaustive exhaustive-slow
