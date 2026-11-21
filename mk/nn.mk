# nn-check compares flexnn with the TFLite interpreter, calling one library per
# rounding mode from Python. uv makes the venv.

NN_VENV ?= $(BUILD)/nn-venv
NN_PY   := $(NN_VENV)/bin/python
NN_SRCS  = test/nn/shim.c $(filter nn/% math/% utils/%,$(LIB_SRCS))
NN_LIBS  = $(foreach m,0 1 2,$(BUILD)/nn-check/libfnn$(m).so)

$(NN_PY): test/nn/requirements.txt
	uv venv -q --allow-existing --python 3.12 $(NN_VENV)
	VIRTUAL_ENV=$(NN_VENV) uv pip install -q -r $<
	@touch $@

$(BUILD)/nn-check/libfnn%.so: $(NN_SRCS) $(wildcard nn/include/*.h arch/*.h)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(STD) $(WARN) $(WERROR) -O2 -fPIC -shared -DFNN_ROUNDING=$* \
		$(LIB_INC) $(NN_SRCS) -lm -o $@

nn-check: $(NN_PY) $(NN_LIBS)
	$(NN_PY) test/nn/check.py $(BUILD)/nn-check

.PHONY: nn-check
