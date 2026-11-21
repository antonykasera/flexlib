# flexlib

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/release/antonykasera/flexlib)](https://github.com/antonykasera/flexlib/releases/latest)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

flexlib is a set of C libraries for Cortex-M microcontrollers. It holds the
pieces most firmware ends up writing for itself, math for chips without an
FPU, and signal processing:

- **[flexutils](utils/README.md)** (`fu_`): bit operations, ring buffers,
  number formatting and parsing, CRCs, COBS, debouncing, random numbers.
- **[flexmath](math/README.md)** (`fm_`): fixed point (Q15, Q31, Q16.16),
  float functions for chips without an FPU, vectors, matrices, quaternions,
  PID.
- **[flexdsp](dsp/README.md)** (`fd_`): filters, FFTs, windows, tone
  detection, correlation, adaptive filters and sample-rate conversion, in
  fixed point and float.

It is C11 with no dependencies and no dynamic memory. Every function states
how accurate it is, and each library's README shows how many cycles it takes
on an STM32F103 (Cortex-M3, 72 MHz), next to newlib and CMSIS-DSP measured
the same way.

## Your first program

This computes the square root of 2 in Q16.16 fixed point and prints it:

```c
#include <stdio.h>
#include <string.h>

#include "fm_sqrt.h"
#include "fu_fmt.h"

int main(void) {
  char buf[FU_FMT_Q16_SIZE];
  fm_q16 r = fm_sqrt_q16(2 * FM_Q16_ONE); /* sqrt(2) in Q16.16, rounded */
  fu_fmt_q16(buf, r, 5U);/* 5 decimal places */
  puts(buf);
  return strcmp(buf, "1.41422") == 0 ? 0 : 1;
}
```

Output:

```text
1.41422
```

`fm_sqrt_q16` rounds the root to the nearest 2^-16, and `fu_fmt_q16` prints
that value to five decimals, the same digits `printf("%.5f")` would give,
without pulling `printf` in. To try it on a PC, save it as `first.c` in the
flexlib directory and build it with the library sources:

```sh
cc -std=c11 -Iarch -Iutils/include -Imath/include -Idsp/include first.c \
   utils/src/*.c math/src/*.c dsp/src/*.c -lm -o first
```

## Using it

Put flexlib in `lib/flexlib`, as a submodule pinned to a release or from the
release archive:

```sh
git submodule add https://github.com/antonykasera/flexlib.git lib/flexlib
git -C lib/flexlib checkout v0.6.0
# or
tar xf flexlib-0.6.0.tar.gz -C lib
```

With [flex](https://github.com/antonykasera/STM32F1-HAL) v1.1.2 or later,
name the libraries you want in flex's project template:

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

With CMake, add the directory and link the library you use. Each target
brings the ones it depends on, and your toolchain and MCU flags apply to
flexlib as to the rest of your code:

```cmake
set(FLEXLIB_REQUIRE 0.6.0)                   # optional: stop if lib/flexlib is older
set(FLEXLIB_DEFINES "FU_CRC_SMALL")          # optional: the build options
add_subdirectory(lib/flexlib)
target_link_libraries(app PRIVATE flexlib::dsp)   # or flexlib::math, flexlib::utils
```

CMake support is on master and comes with the next release. The v0.6.0
archive does not include `CMakeLists.txt` yet.

Build with `-ffunction-sections -fdata-sections` and link with
`--gc-sections`, so the functions you don't call stay out of the image. A
failed argument check traps by default. Define `FX_ASSERT_FAIL(file, line)`
to report it your own way, or `FX_DISABLE_ASSERT` to remove the checks. Each
library's README lists its other options.

If you set `FLEXLIB_REQUIRE := 0.6.0` before the include, the build stops
when `lib/flexlib` is older than that.

To upgrade, read the CHANGELOGs, then check out the new tag or unpack the new
archive. Before 1.0, a minor release may change an API. A patch release does
not.

## Versions

Each library has its own version and tags (`flexdsp-v0.4.0`). A repository
release (`v0.6.0`) bundles one of each: flexutils 0.1.2, flexmath 0.2.5,
flexdsp 0.4.0. The `fx_` prefix is the shared layer under all three.

Planned: flexctrl (`fct_`, control), flexcomm (`fco_`, protocols), flexnn
(`fnn_`, int8 inference), flextrace (`ftr_`, logging).

## Testing

`make` runs the host tests, the cross builds for Cortex-M3, M4 and M7, the
lint pass and the disassembly checks. [docs/testing.md](docs/testing.md)
lists every command and what each module is tested against.
[bench/](https://github.com/antonykasera/flexlib/blob/master/bench/README.md)
is the firmware that times each function on the board. The results are in
[docs/results/f103.md](docs/results/f103.md), and the errors next to
CMSIS-DSP's in [docs/accuracy.md](docs/accuracy.md).

## License

Apache License 2.0, see [LICENSE](LICENSE). If you ship flexlib in a product,
include the attribution in [NOTICE](NOTICE) with the product's other
third-party notices (section 4(d) of the license).

Releases up to v0.4.0 were published under the MIT License. The copyright
holder also grants every earlier release and commit under Apache-2.0.
