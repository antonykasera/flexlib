# nn-check compares flexnn with the TFLite interpreter, calling one library per
# rounding mode from Python. uv makes the venv.

NN_VENV ?= $(BUILD)/nn-venv
NN_PY   := $(NN_VENV)/bin/python
# bin/python links to uv's interpreter, so the stamp is a file of its own
NN_DONE := $(NN_VENV)/.installed
NN_SRCS  = test/nn/shim.c $(filter nn/% math/% utils/%,$(LIB_SRCS))
NN_LIBS  = $(foreach m,0 1 2 3,$(BUILD)/nn-check/libfnn$(m).so)

$(NN_DONE): test/nn/requirements.txt
	uv venv -q --allow-existing --python 3.12 $(NN_VENV)
	VIRTUAL_ENV=$(NN_VENV) uv pip install -q -r $<
	@touch $@

$(BUILD)/nn-check/libfnn%.so: $(NN_SRCS) $(wildcard nn/include/*.h arch/*.h)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -fPIC -shared -DFNN_ROUNDING=$* \
		$(LIB_INC) $(NN_SRCS) -lm -o $@

nn-check: $(NN_DONE) $(NN_LIBS)
	$(NN_PY) test/nn/check.py $(BUILD)/nn-check

.PHONY: nn-check

# nn-cmsis runs flexnn in FNN_ROUND_GEMMLOWP against CMSIS-NN built for the host.
NN_CMSIS_DIR  ?= third_party/cmsis-nn
NN_CMSIS_SRCS  = $(filter-out %_f16.c %_f32.c,$(wildcard $(NN_CMSIS_DIR)/Source/*/*.c))
NN_CMSIS_OBJS  = $(patsubst $(NN_CMSIS_DIR)/Source/%.c,$(BUILD)/nn-cmsis/%.o,$(NN_CMSIS_SRCS))
NN_CMSIS_BIN   = $(BUILD)/nn-cmsis/cmsis_check

$(BUILD)/nn-cmsis/%.o: $(NN_CMSIS_DIR)/Source/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(STD) -O2 -w -isystem $(NN_CMSIS_DIR)/Include -c $< -o $@

$(NN_CMSIS_BIN): test/nn/cmsis.c $(filter nn/%,$(LIB_SRCS)) $(wildcard nn/include/*.h) \
                 $(NN_CMSIS_OBJS)
	$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -DFNN_ROUNDING=FNN_ROUND_GEMMLOWP \
		$(LIB_INC) -isystem $(NN_CMSIS_DIR)/Include test/nn/cmsis.c \
		$(filter nn/%,$(LIB_SRCS)) $(NN_CMSIS_OBJS) -lm -o $@

ifeq ($(NN_CMSIS_SRCS),)
nn-cmsis:
	@echo "nn-cmsis needs CMSIS-NN: run make deps"
	@exit 1
else
nn-cmsis: $(NN_CMSIS_BIN)
	$<
endif

.PHONY: nn-cmsis
