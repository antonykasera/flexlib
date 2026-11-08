# flexutils.mk — include fragment for consuming flexutils as a library.
#
# Add flexlib to your project (as a git submodule under lib/flexlib) and, in
# your own Makefile:
#
#     include lib/flexlib/flexutils.mk
#
#     CFLAGS += $(FLEXLIB_CFLAGS)   # include paths only; your MCU flags apply
#     SRCS   += $(FU_SRCS)
#
# This fragment exports VARIABLES only. flexlib has no IRQ handlers, so build
# with -ffunction-sections -fdata-sections and link with --gc-sections, and
# the functions you don't call drop out.

# Directory this fragment lives in.
FLEXLIB_ROOT := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

FLEXLIB_CFLAGS := -I$(FLEXLIB_ROOT)/include -I$(FLEXLIB_ROOT)/arch

FU_SRCS := $(wildcard $(FLEXLIB_ROOT)/utils/src/*.c)
