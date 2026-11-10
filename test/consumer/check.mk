
include flexdsp.mk
include flexmath.mk
include flexutils.mk
include flexlib.mk

ifneq ($(words $(FLEXLIB_SRCS)),$(words $(sort $(FLEXLIB_SRCS))))
$(error flexlib fragments: a source is listed twice)
endif
ifneq ($(words $(FLEXLIB_CFLAGS)),$(words $(sort $(FLEXLIB_CFLAGS))))
$(error flexlib fragments: an include path is listed twice)
endif
ifneq ($(sort $(FLEXLIB_SRCS)),$(sort $(FU_SRCS) $(FM_SRCS) $(FD_SRCS)))
$(error flexlib fragments: FLEXLIB_SRCS is not FU_SRCS + FM_SRCS + FD_SRCS)
endif

all:
	@echo "consumer fragments: $(words $(FLEXLIB_SRCS)) sources, $(FLEXLIB_CFLAGS)"
