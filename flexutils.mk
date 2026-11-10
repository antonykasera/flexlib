# flexutils.mk — flexutils as a library.
#
#     include lib/flexlib/flexutils.mk
#
#     CFLAGS += $(FLEXLIB_CFLAGS)   # include paths only; your MCU flags apply
#     SRCS   += $(FU_SRCS)          # $(FLEXLIB_SRCS): every flexlib library included
#
# Build with -ffunction-sections -fdata-sections and link with --gc-sections.

ifndef FLEXUTILS_MK
FLEXUTILS_MK := 1

FLEXLIB_ROOT := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

FLEXLIB_VERSION := $(shell awk '/define FLEXLIB_VERSION_(MAJOR|MINOR|PATCH) / \
  { printf "%s%s", s, $$3; s = "." }' $(FLEXLIB_ROOT)/arch/fx_version.h)
ifneq ($(FLEXLIB_REQUIRE),)
ifneq ($(shell echo $(FLEXLIB_VERSION) $(FLEXLIB_REQUIRE) | awk '{ \
  split($$1, h, "."); split($$2, r, "."); \
  print (h[1] * 10000 + h[2] * 100 + h[3] >= r[1] * 10000 + r[2] * 100 + r[3]) }'),1)
$(error flexlib $(FLEXLIB_VERSION) in $(FLEXLIB_ROOT) is older than FLEXLIB_REQUIRE $(FLEXLIB_REQUIRE))
endif
endif

FLEXLIB_CFLAGS := $(FLEXLIB_CFLAGS) -I$(FLEXLIB_ROOT)/arch \
                  -I$(FLEXLIB_ROOT)/utils/include
FU_SRCS := $(wildcard $(FLEXLIB_ROOT)/utils/src/*.c)
FLEXLIB_SRCS := $(FLEXLIB_SRCS) $(FU_SRCS)

endif
