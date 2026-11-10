# flexdsp.mk — flexdsp as a library (pulls in flexmath.mk and flexutils.mk).
#
#     include lib/flexlib/flexdsp.mk
#
#     CFLAGS += $(FLEXLIB_CFLAGS)
#     SRCS   += $(FLEXLIB_SRCS)       # flexdsp, flexmath and flexutils

ifndef FLEXDSP_MK
FLEXDSP_MK := 1

include $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))/flexmath.mk

FLEXLIB_CFLAGS := $(FLEXLIB_CFLAGS) -I$(FLEXLIB_ROOT)/dsp/include
FD_SRCS := $(wildcard $(FLEXLIB_ROOT)/dsp/src/*.c)
FLEXLIB_SRCS := $(FLEXLIB_SRCS) $(FD_SRCS)

endif
