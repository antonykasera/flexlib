# flexnn.mk — flexnn as a library (pulls in flexmath.mk and flexutils.mk).
#
#     include lib/flexlib/flexnn.mk
#
#     CFLAGS += $(FLEXLIB_CFLAGS)
#     SRCS   += $(FLEXLIB_SRCS)       # flexnn, flexmath and flexutils

ifndef FLEXNN_MK
FLEXNN_MK := 1

include $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))/flexmath.mk

FLEXLIB_CFLAGS := $(FLEXLIB_CFLAGS) -I$(FLEXLIB_ROOT)/nn/include
FNN_SRCS := $(wildcard $(FLEXLIB_ROOT)/nn/src/*.c)
FLEXLIB_SRCS := $(FLEXLIB_SRCS) $(FNN_SRCS)

endif
