# flexlib

flexlib is a set of C11 libraries for Cortex-M microcontrollers: the
utilities most firmware writes for itself, fixed-point and float math for
parts without an FPU, and signal processing. They need no dynamic memory and
no other library. Every function states how accurate it is, and every page
shows how many cycles it takes on an STM32F103 (Cortex-M3, 72 MHz), next to
newlib and CMSIS-DSP measured the same way.

| library | prefix | contents |
|---|---|---|
| [flexutils](utils/index.md) | `fu_` | bits, formatting, parsing, CRCs, rings, COBS, debouncing, PRNGs |
| [flexmath](math/index.md) | `fm_` | fixed point, float functions without an FPU, vectors, matrices, quaternions, PID |
| [flexdsp](dsp/index.md) | `fd_` | FIR and biquad filters, windows, tones, noise, FFTs, correlation, LMS, resampling |

## Install

Put flexlib in `lib/flexlib`, as a submodule pinned to a release or from the
release archive:

```sh
git submodule add https://github.com/antonykasera/flexlib.git lib/flexlib
git -C lib/flexlib checkout v@VERSION@
```

Include the fragment; it only sets variables:

```make
include lib/flexlib/flexlib.mk    # FLEXLIB_LIBS ?= utils math dsp
CFLAGS += $(FLEXLIB_CFLAGS)
SRCS   += $(FLEXLIB_SRCS)
```

In a [flex](https://github.com/antonykasera/STM32F1-HAL) project,
`FLEXLIB_LIBS := math` in the Makefile does the same. Build with
`-ffunction-sections -fdata-sections` and link with `--gc-sections`.

## A first program

This computes $sqrt 2$ in Q16.16 and prints it with five decimals:

```{literalinclude} examples/flexlib.c
:language: c
```

Which prints

```{literalinclude} examples/flexlib.out
:language: text
```

[Get started](get-started.md) shows how to build it, and the options.

```{toctree}
:hidden:

get-started
utils/index
math/index
dsp/index
results/index
design/index
```
