# Documentation site: Sphinx + Breathe over the Doxygen XML of docs-check

SITE_DIR := docs/site
SITE_VENV ?= build/site-venv
SITE_PY := $(SITE_VENV)/bin/python
SITE_EXAMPLES := $(wildcard $(SITE_DIR)/examples/*.c)

$(SITE_PY): $(SITE_DIR)/requirements.txt
	uv venv -q --allow-existing $(SITE_VENV)
	VIRTUAL_ENV=$(SITE_VENV) uv pip install -q -r $<
	@touch $@

# every example compiles -Werror against the library sources and runs; its
# output must match the .out file next to it, which the pages show
site-examples:
	@mkdir -p build/site-examples
	@for f in $(SITE_EXAMPLES); do \
	  b=build/site-examples/$$(basename $$f .c); \
	  $(HOST_CC) -std=c11 -Wall -Wextra -Wconversion -Wsign-conversion -Werror \
	    -Iarch -Iutils/include -Imath/include -Idsp/include $$f \
	    $(FU_SRCS) $(FM_SRCS) $(FD_SRCS) -o $$b -lm && $$b > $$b.txt || exit 1; \
	  if [ -f $${f%.c}.out ]; then \
	    diff -u $${f%.c}.out $$b.txt || { echo "$$f: output differs"; exit 1; }; \
	  fi; \
	done
	@echo "Site examples: $(words $(SITE_EXAMPLES)) ok"

site: docs-check site-examples $(SITE_PY)
	$(SITE_PY) -m sphinx -W --keep-going -q -b html $(SITE_DIR) build/site/html
	@echo "Site: build/site/html/index.html"

.PHONY: site site-examples
