# flexlib

C11 libraries for Cortex-M, without allocation or dependencies. Each
function's header states its error bound; the module pages give cycle counts
measured on an STM32F103 (Cortex-M3, 72 MHz) next to newlib and CMSIS-DSP.

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
git -C lib/flexlib checkout v0.5.0
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

## Example

```{literalinclude} examples/flexlib.c
:language: c
```

```{toctree}
:hidden:

utils/index
math/index
dsp/index
```
