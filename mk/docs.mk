# API documentation: Doxygen over the headers whose comments are complete

# fm_core.h and fm_sum.h are flexmath internals that flexdsp shares
DOCS_HEADERS := $(wildcard utils/include/*.h) \
  $(filter-out math/include/fm_core.h math/include/fm_sum.h, \
    $(wildcard math/include/*.h)) \
  $(wildcard dsp/include/*.h)

docs-check:
	@rm -rf build/doxygen
	@mkdir -p build/doxygen
	FLEXLIB_VERSION=$(FLEXLIB_VERSION) DOCS_INPUT="$(DOCS_HEADERS)" doxygen docs/Doxyfile
	@echo "API docs: build/doxygen/html/index.html ($(words $(DOCS_HEADERS)) headers)"

.PHONY: docs-check
include mk/site.mk
