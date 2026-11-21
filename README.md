# flexlib

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/release/antonykasera/flexlib)](https://github.com/antonykasera/flexlib/releases/latest)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

C libraries for Cortex-M microcontrollers:

- **[flexutils](utils/README.md)** (`fu_`): bit operations, ring buffers,
  number formatting and parsing, CRCs, COBS, debouncing, random numbers.
- **[flexmath](math/README.md)** (`fm_`): fixed point (Q15, Q31, Q16.16),
  float functions for chips without an FPU, vectors, matrices, quaternions,
  PID.
- **[flexdsp](dsp/README.md)** (`fd_`): filters, FFTs, windows, tone
  detection, correlation, adaptive filters and sample-rate conversion, in
  fixed point and float.

The code is C11 with no dependencies and no dynamic allocation. Each
function documents its error bound. Its speed is measured in CPU cycles on an
STM32F103 (Cortex-M3, 72 MHz), next to newlib and CMSIS-DSP. Each library's
README has the results.

## Using it

Put flexlib in `lib/flexlib`, as a submodule pinned to a release or from the
release archive:

```sh
git submodule add https://github.com/antonykasera/flexlib.git lib/flexlib
git -C lib/flexlib checkout v0.5.0
# or
tar xf flexlib-0.5.0.tar.gz -C lib
```

With [flex](https://github.com/antonykasera/STM32F1-HAL) v1.1.2 or later,
name the libraries in flex's project template:

```make
FLEX_MODULES := gpio clock usb
FLEXLIB_LIBS := math              # utils, math, dsp; each brings the ones it uses
```

Without flex, include the fragment yourself. It only sets variables:

```make
include lib/flexlib/flexlib.mk    # FLEXLIB_LIBS ?= utils math dsp
CFLAGS += $(FLEXLIB_CFLAGS)       # include paths only
SRCS   += $(FLEXLIB_SRCS)
```

Build with `-ffunction-sections -fdata-sections` and link with
`--gc-sections`. Asserts call `FX_ASSERT_FAIL(file, line)` if you define it.
`FX_DISABLE_ASSERT` removes them. Each library's README lists its other
options.

`FLEXLIB_REQUIRE := 0.5.0` before the include stops the build when
`lib/flexlib` is older.

To upgrade, read the CHANGELOGs, then check out the new tag or unpack the new
archive. Before 1.0, a minor release may change an API. A patch release does
not.

## Versions

Each library has its own version and tags (`flexdsp-v0.3.0`). A repository
release (`v0.5.0`) bundles them: flexutils 0.1.2, flexmath 0.2.4, flexdsp
0.3.0. The `fx_` prefix is the shared layer under all three.

Planned: flexctrl (`fct_`, control), flexcomm (`fco_`, protocols), flexnn
(`fnn_`, int8 inference), flextrace (`ftr_`, logging).

## Testing

`make` runs the host tests, the cross builds, the lint pass and the
disassembly checks. [docs/testing.md](docs/testing.md) lists every command
and what each module is tested against.
[bench/](https://github.com/antonykasera/flexlib/blob/master/bench/README.md)
is the firmware that times each function on the board. The results are in
[docs/results/f103.md](docs/results/f103.md), and the errors next to
CMSIS-DSP's are in [docs/accuracy.md](docs/accuracy.md).

## License

Apache License 2.0, see [LICENSE](LICENSE). If you ship flexlib in a product,
include the attribution in [NOTICE](NOTICE) with the product's other
third-party notices (section 4(d) of the license).

Releases up to v0.4.0 were published under the MIT License. The copyright
holder also grants every earlier release and commit under Apache-2.0.
