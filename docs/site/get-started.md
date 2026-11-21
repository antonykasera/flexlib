# Get started

flexlib is plain C11 source with no dependencies and no dynamic allocation.
A project compiles the sources it uses next to its own. These steps add it to
a Make project, build a first program on the host and set the options.

## Add flexlib

Put flexlib in `lib/flexlib`, as a submodule pinned to a release:

```sh
git submodule add https://github.com/antonykasera/flexlib.git lib/flexlib
git -C lib/flexlib checkout v@VERSION@
```

Or unpack the release archive from the
[releases page](https://github.com/antonykasera/flexlib/releases):

```sh
tar xf flexlib-@VERSION@.tar.gz -C lib
```

## Build it with Make

Include the fragment. It only sets variables:

```make
FLEXLIB_LIBS := math               # utils, math, dsp; the default is all three
FLEXLIB_REQUIRE := @VERSION@       # optional: stop if lib/flexlib is older
include lib/flexlib/flexlib.mk

CFLAGS += $(FLEXLIB_CFLAGS)        # include paths only
SRCS   += $(FLEXLIB_SRCS)
```

Each library brings the ones it uses: flexdsp brings flexmath, and flexmath
brings flexutils. `FLEXLIB_SRCS` lists the `.c` files of the libraries named,
and your own compiler flags apply to them.

Compile with `-ffunction-sections -fdata-sections` and link with
`--gc-sections`, so functions you do not call are left out of the image.

In a [flex](https://github.com/antonykasera/STM32F1-HAL) project (v1.1.2 or
later), set `FLEXLIB_LIBS` in the project Makefile. The template does the
rest.

## A first program

This program computes $sqrt 2$ in Q16.16 and prints it with five decimals:

```{literalinclude} examples/flexlib.c
:language: c
```

Save it as `first.c` and build it on the host, from the flexlib directory:

```sh
cc -std=c11 -Iarch -Iutils/include -Imath/include -Idsp/include first.c \
   utils/src/*.c math/src/*.c dsp/src/*.c -lm -o first
./first
```

Which prints

```{literalinclude} examples/flexlib.out
:language: text
```

{c:func}`fm_sqrt_q16` rounds the root to the nearest 2^-16, and
{c:func}`fu_fmt_q16` prints that exact value to five decimals, as `printf`
would. The same source builds for a Cortex-M target, where `puts` needs a
console.

## Options

Define these for the whole build, for example `CFLAGS += -DFX_DISABLE_ASSERT`.

`FX_ASSERT_FAIL(file, line)`
: Called when an argument check fails. By default a failed check traps,
  with no stdio.

`FX_DISABLE_ASSERT`
: Removes the argument checks.

`FU_CRC_SMALL`
: 16-entry CRC tables instead of 256-entry ones: less flash, slower.

`FM_LUT_IN_RAM`
: Puts flexmath's tables (2.8 KB) in `.data`: a call that misses the flash
  cache is faster.

`FD_FFT_MAX`, `FD_LUT_IN_RAM`
: The largest FFT size, and where its table goes. See [flexdsp](dsp/index.md).

## Upgrade

Read the [changelog](https://github.com/antonykasera/flexlib/blob/master/CHANGELOG.md),
then check out the new tag or unpack the new archive. Before 1.0, a minor
release may change an API. A patch release does not.

Each library also has its own version and tags (`flexutils-v`, `flexmath-v`,
`flexdsp-v`). A repository release bundles one version of each.
