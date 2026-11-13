# API documentation: Doxygen over the headers whose comments are complete

DOCS_HEADERS := $(wildcard utils/include/*.h) \
  $(addprefix math/include/,fm_fixed.h fm_sqrt.h fm_div.h fm_trig.h \
    fm_atan.h fm_exp.h) \
  $(wildcard dsp/include/*.h)

docs-check:
	@rm -rf build/doxygen
	@mkdir -p build/doxygen
	FLEXLIB_VERSION=$(FLEXLIB_VERSION) DOCS_INPUT="$(DOCS_HEADERS)" doxygen docs/Doxyfile
	@echo "API docs: build/doxygen/html/index.html ($(words $(DOCS_HEADERS)) headers)"

.PHONY: docs-check
include mk/site.mk
