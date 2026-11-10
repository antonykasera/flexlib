# The same project built for the host: flexlib.mk

HOST_CC ?= gcc

include lib/flexlib/flexlib.mk

all: build/host/app
	./build/host/app

build/host/app: main.c $(FLEXLIB_SRCS)
	@mkdir -p $(dir $@)
	$(HOST_CC) -std=c11 -Wall -Wextra -Wconversion -Werror -O2 \
		-ffunction-sections -fdata-sections -Wl,--gc-sections \
		$(FLEXLIB_CFLAGS) $^ -o $@

.PHONY: all
