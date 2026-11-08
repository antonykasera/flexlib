# flexmath.mk — include fragment for consuming flexmath as a library.
#
#     include lib/flexlib/flexmath.mk
#
#     CFLAGS += $(FLEXLIB_CFLAGS)
#     SRCS   += $(FM_SRCS) $(FU_SRCS)   # flexmath uses flexutils
#
# Pulls in flexutils.mk, so FU_SRCS and FLEXLIB_CFLAGS are set too. Same
# gc-sections note as there.

FLEXLIB_ROOT := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
include $(FLEXLIB_ROOT)/flexutils.mk

FM_SRCS := $(wildcard $(FLEXLIB_ROOT)/math/src/*.c)
