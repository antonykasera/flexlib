# host tests, and the exhaustive checks (every 32-bit input, on all cores)

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

.PHONY: host-test exhaustive exhaustive-slow
