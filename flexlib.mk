# flexlib.mk
#
#     FLEXLIB_LIBS := utils math dsp  # optional: the default is all of them
#     FLEXLIB_REQUIRE := 0.4.0        # optional: stop if lib/flexlib is older
#     include lib/flexlib/flexlib.mk
#
#     CFLAGS += $(FLEXLIB_CFLAGS)
#     SRCS   += $(FLEXLIB_SRCS)

FLEXLIB_LIBS ?= utils math dsp

include $(foreach l,$(FLEXLIB_LIBS),\
          $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))/flex$(l).mk)
