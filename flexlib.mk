# flexlib.mk — every flexlib library you name, in one include.
#
#     FLEXLIB_LIBS := utils math      # optional: the default is all of them
#     include lib/flexlib/flexlib.mk
#
#     CFLAGS += $(FLEXLIB_CFLAGS)
#     SRCS   += $(FLEXLIB_SRCS)

FLEXLIB_LIBS ?= utils math

include $(foreach l,$(FLEXLIB_LIBS),\
          $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))/flex$(l).mk)
