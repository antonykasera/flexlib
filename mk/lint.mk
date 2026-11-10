# -Wpedantic over everything, and each header compiled on its own

LINT_FLAGS   = $(STD) $(WARN) -Wpedantic -Werror -fsyntax-only
LINT_HEADERS = $(wildcard arch/*.h utils/include/*.h math/include/*.h \
                          utils/src/*.h math/src/*.h) bench/core/bench.h

lint:
	$(ARM_CC) $(M3_FLAGS) $(LINT_FLAGS) $(LIB_INC) $(BENCH_INC) $(CROSS_SRCS) \
		$(BENCH_PORTABLE) bench/core/golden.c
	$(HOST_CC) $(LINT_FLAGS) $(LIB_INC) -Itest/host $(wildcard test/host/*.c)
	$(HOST_CC) $(LINT_FLAGS) -DBENCH_HOST $(LIB_INC) $(BENCH_INC) \
		$(BENCH_PORTABLE) bench/core/golden_gen.c
	$(ARM_CC) $(M3_FLAGS) $(LINT_FLAGS) -DFU_CRC_SMALL $(LIB_INC) utils/src/fu_crc.c
	$(ARM_CC) $(M3_FLAGS) $(LINT_FLAGS) -DFM_LUT_IN_RAM $(LIB_INC) math/src/fm_tables.c
	@for h in $(LINT_HEADERS); do \
		echo "  header $$h"; \
		echo "#include \"$$h\"" | $(ARM_CC) $(M3_FLAGS) $(LINT_FLAGS) $(LIB_INC) \
			-I. -x c - || exit 1; \
	done

.PHONY: lint
