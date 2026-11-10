# flexmath.mk — flexmath as a library (pulls in flexutils.mk).
#
#     include lib/flexlib/flexmath.mk
#
#     CFLAGS += $(FLEXLIB_CFLAGS)
#     SRCS   += $(FLEXLIB_SRCS)       # flexmath and flexutils, which it uses

ifndef FLEXMATH_MK
FLEXMATH_MK := 1

include $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))/flexutils.mk

FLEXLIB_CFLAGS := $(FLEXLIB_CFLAGS) -I$(FLEXLIB_ROOT)/math/include
FM_SRCS := $(wildcard $(FLEXLIB_ROOT)/math/src/*.c)
FLEXLIB_SRCS := $(FLEXLIB_SRCS) $(FM_SRCS)

endif
