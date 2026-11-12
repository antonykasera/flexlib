# API documentation: Doxygen over the headers whose comments are complete

DOCS_HEADERS := utils/include/fu_crc.h

docs-check:
	@rm -rf build/doxygen
	@mkdir -p build/doxygen
	FLEXLIB_VERSION=$(FLEXLIB_VERSION) DOCS_INPUT="$(DOCS_HEADERS)" doxygen docs/Doxyfile
	@echo "API docs: build/doxygen/html/index.html ($(words $(DOCS_HEADERS)) headers)"

.PHONY: docs-check
include mk/site.mk
