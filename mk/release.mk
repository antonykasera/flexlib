# deps: the submodules for the bench and accuracy (projects using flexlib skip them)
# dist: the release archive, only what you need
# consumer: a blank project builds against that archive (flex's template, host gcc)

DIST_NAME  = flexlib-$(FLEXLIB_VERSION)
DIST       = $(BUILD)/dist/$(DIST_NAME).tar.gz
DIST_FILES = LICENSE NOTICE README.md CHANGELOG.md flexlib.mk flexutils.mk flexmath.mk \
             flexdsp.mk \
             $(wildcard arch/*.h utils/*.md utils/include/*.h utils/src/*.[ch] \
                        math/*.md math/include/*.h math/src/*.[ch] \
                        dsp/*.md dsp/include/*.h dsp/src/*.[ch]) \
             docs/README.md docs/accuracy.md $(wildcard docs/results/*.md)

CONSUMER = $(BUILD)/consumer

deps:
	git submodule update --init --checkout third_party/flex third_party/cmsis-dsp

dist:
	@mkdir -p $(BUILD)/dist
	tar --sort=name --owner=0 --group=0 --numeric-owner \
		--transform 's,^,flexlib/,' -cf - $(DIST_FILES) | gzip -n > $(DIST)
	cd $(BUILD)/dist && sha256sum $(DIST_NAME).tar.gz > $(DIST_NAME).tar.gz.sha256
	@echo "$(DIST): $(words $(DIST_FILES)) files, $$(du -h $(DIST) | cut -f1)"

consumer: dist
	@test -f third_party/flex/flex.mk || { echo "third_party/flex is missing: make deps"; exit 1; }
	$(MAKE) -s -f test/consumer/check.mk
	rm -rf $(CONSUMER) && mkdir -p $(CONSUMER)/lib
	tar xzf $(DIST) -C $(CONSUMER)/lib
	ln -s $(CURDIR)/third_party/flex $(CONSUMER)/lib/flex
	cp third_party/flex/template/Makefile third_party/flex/template/linker.ld \
		test/consumer/main.c $(CONSUMER)/
	$(MAKE) -C $(CONSUMER) FLEXLIB_LIBS=dsp FLEXLIB_REQUIRE=$(FLEXLIB_VERSION) \
		EXTRA_CFLAGS=-Werror
	$(MAKE) -s -C $(CONSUMER) FLEXLIB_LIBS=dsp FLEXLIB_REQUIRE=99.0.0 2>&1 | \
		grep -q "older than FLEXLIB_REQUIRE 99.0.0"
	$(MAKE) -C $(CONSUMER) -f $(CURDIR)/test/consumer/host.mk HOST_CC=$(HOST_CC)

.PHONY: deps dist consumer
