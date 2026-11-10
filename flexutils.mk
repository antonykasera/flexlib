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

FLEXLIB_CFLAGS := $(FLEXLIB_CFLAGS) -I$(FLEXLIB_ROOT)/arch \
                  -I$(FLEXLIB_ROOT)/utils/include
FU_SRCS := $(wildcard $(FLEXLIB_ROOT)/utils/src/*.c)
FLEXLIB_SRCS := $(FLEXLIB_SRCS) $(FU_SRCS)

endif
