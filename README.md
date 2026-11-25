# flexlib

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/release/antonykasera/flexlib)](https://github.com/antonykasera/flexlib/releases/latest)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

C libraries for Cortex-M microcontrollers, covering fixed-point and float math
without an FPU, signal processing, and int8 neural-network inference. Written
in C11 with no external dependencies and no dynamic allocation.

## Libraries

| Library | Prefix | Contents |
| --- | --- | --- |
| [flexutils](utils/README.md) | `fu_` | Bit operations, ring buffers, number formatting and parsing, CRCs, COBS, debouncing, random numbers |
| [flexmath](math/README.md) | `fm_` | Q15, Q31 and Q16.16 arithmetic, float functions on integer instructions, vectors, matrices, quaternions, PID |
| [flexdsp](dsp/README.md) | `fd_` | Filters, FFTs, windows, tone detection, correlation, adaptive filters, resampling, speech features |
| [flexnn](nn/README.md) | `fnn_` | int8 inference with TFLite's arithmetic, and a `.tflite` to C converter |

Error bounds are documented in the public headers. Cycle counts measured on an
STM32F103 (Cortex-M3, 72 MHz) are in [docs/results/f103.md](docs/results/f103.md).

## Requirements

| Component | Version |
| --- | --- |
| C compiler | C11, tested with arm-none-eabi-gcc 13 and 16, host gcc and clang |
| CMake (optional) | 3.16 or later |
| Python (flexnn converter) | 3.10 or later |
| [flex](https://github.com/antonykasera/STM32F1-HAL) (optional) | 1.1.2 or later |

## Installation

Add the repository as a submodule pinned to a release:

```sh
git submodule add https://github.com/antonykasera/flexlib.git lib/flexlib
git -C lib/flexlib checkout v0.11.0
```

Or extract the release archive:

```sh
tar xf flexlib-0.11.0.tar.gz -C lib
```

## Usage

### Make

With the flex project template, list the libraries in `FLEXLIB_LIBS`.
Dependencies are added automatically.

```make
FLEXLIB_LIBS := math
```

In other Make projects, include the fragment. It defines variables and no
rules.

```make
include lib/flexlib/flexlib.mk    # FLEXLIB_LIBS ?= utils math dsp nn
CFLAGS += $(FLEXLIB_CFLAGS)
SRCS   += $(FLEXLIB_SRCS)
```

### CMake

```cmake
set(FLEXLIB_REQUIRE 0.11.0)          # optional minimum version
set(FLEXLIB_DEFINES "FU_CRC_SMALL")  # optional build options
add_subdirectory(lib/flexlib)
target_link_libraries(app PRIVATE flexlib::dsp)
```

The targets are `flexlib::utils`, `flexlib::math`, `flexlib::dsp` and
`flexlib::nn`, and they link their dependencies. `FetchContent` also works with
the release archive:

```cmake
include(FetchContent)
FetchContent_Declare(flexlib
  URL https://github.com/antonykasera/flexlib/releases/download/v0.11.0/flexlib-0.11.0.tar.gz
  URL_HASH SHA256=...)   # from flexlib-0.11.0.tar.gz.sha256
FetchContent_MakeAvailable(flexlib)
```

Compile with `-ffunction-sections -fdata-sections` and link with
`--gc-sections` to remove unused functions.

### Example

```c
#include <stdio.h>

#include "fm_sqrt.h"
#include "fu_fmt.h"

int main(void) {
  char buf[FU_FMT_Q16_SIZE];
  fm_q16 r = fm_sqrt_q16(2 * FM_Q16_ONE); /* sqrt(2) in Q16.16 */
  fu_fmt_q16(buf, r, 5U);
  puts(buf); /* 1.41422 */
  return 0;
}
```

Saved as `first.c` in the repository root, it builds on a host with:

```sh
cc -std=c11 -Iarch -Iutils/include -Imath/include first.c \
   utils/src/*.c math/src/*.c -o first
```

## Configuration

| Macro | Effect |
| --- | --- |
| `FX_ASSERT_FAIL(file, line)` | Handler for failed argument checks. The default traps. |
| `FX_DISABLE_ASSERT` | Removes argument checks. |
| `FLEXLIB_REQUIRE` | Minimum flexlib version. The build fails on an older tree. |
| `FU_CRC_SMALL` | 16-entry CRC tables instead of 256-entry tables. |
| `FM_LUT_IN_RAM` | Places the flexmath tables (2.8 KB) in `.data`. |
| `FD_FFT_MAX` | Largest FFT length, 1024 by default. 2048 and 4096 add a 4 KB table. |
| `FNN_ROUNDING` | flexnn rounding mode. See [nn/README.md](nn/README.md#rounding). |

## Versioning

The libraries are versioned and tagged separately (`flexdsp-v0.6.0`). A
repository release (`v0.11.0`) bundles flexutils 0.1.2, flexmath 0.2.5, flexdsp
0.6.0 and flexnn 0.4.0. Before 1.0, minor releases may change the API and
patch releases do not. Changes are listed in [CHANGELOG.md](CHANGELOG.md) and
the per-library changelogs.

## Testing

```sh
make                       # host tests, Cortex-M3/M4/M7 builds, lint, asm-audit
make host-test HOST_CC=clang
```

[docs/testing.md](docs/testing.md) lists the test targets and references.
[bench/](https://github.com/antonykasera/flexlib/blob/master/bench/README.md)
contains the benchmark firmware, and [docs/accuracy.md](docs/accuracy.md) the
measured errors.

## License

Apache License 2.0. See [LICENSE](LICENSE). Products that ship flexlib must
include the attribution in [NOTICE](NOTICE) (section 4(d) of the license).
Releases up to v0.4.0 were published under the MIT License and are also
granted under Apache-2.0.
