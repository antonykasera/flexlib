# flexutils

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexutils-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

Most firmware ends up writing the same small pieces: taking bytes apart,
turning numbers into text and back, checking frames with a CRC, passing bytes
from an interrupt to the main loop, framing packets on a serial link,
debouncing buttons. flexutils has them, in plain C11 with no allocation and
no `printf`.

This prints a temperature held in Q16.16 fixed point, with an id and a
count:

```c
#include <stdio.h>

#include "fu_fmt.h"

int main(void) {
  int32_t temp = 1546650; /* 23.6 in Q16.16 */
  char line[48];
  size_t n = fu_snprintf(line, sizeof line, "t=%.1q C id=%08X n=%u", temp,
                         0xBEEFU, 42U);

  char q[FU_FMT_Q16_SIZE];
  fu_fmt_q16(q, temp, 4U); /* "23.6000", as printf "%.4f" */

  puts(line);
  puts(q);
  return n == 25U ? 0 : 1;
}
```

Which prints

```text
t=23.6 C id=0000BEEF n=42
23.6000
```

`%.1q` is the conversion for Q16.16. The value held is 23.6000061...: one
decimal gives `23.6`, and `fu_fmt_q16` with four digits gives exactly what
`printf("%.4f")` would print for it. `newlib`'s `snprintf` would take several
thousand cycles for the float alone.

## Modules

Cycles are medians on an STM32F103 at 72 MHz (`-O2`, warm cache), from
[docs/results/f103.md](../docs/results/f103.md).

| header | what | cycles | baseline |
|---|---|--:|---|
| `fu_bits.h` | clz, ctz, popcount, bit reverse, byte swaps, LE / BE loads, sign extension | popcount 13 | `__builtin_popcount` 17 |
| `fu_fmt.h` | integers, hex, Q16.16 and float to text, `fu_snprintf` | u32 116, float `%.2f` 182 | `snprintf` 1024, 4285 |
| `fu_parse.h` | text to u32, i32, hex, Q16.16 | u32 153, Q16.16 275 | `strtoul` 327, `strtod` 1332 |
| `fu_crc.h` | CRC-8 SMBus / Maxim, CRC-16 CCITT / MODBUS, CRC-32 | CRC-32 6.2 per byte | bitwise 54 per byte |
| `fu_ring.h` | lock-free byte ring, element queue, DMA receive | 64 B write + read 392 | `memcpy` ring 933 |
| `fu_cobs.h` | COBS encode, in-place decode, byte receiver | 64 B encode 329 | byte loop 996 |
| `fu_debounce.h` | one input, or 32 inputs at once | 32 inputs 20 per tick | 16 integrators 379 |
| `fu_rand.h` | xorshift32, xoshiro128**, unbiased bounded draws | xoshiro128** 22 | `rand()` 44 |

`fu_snprintf` handles `%d %i %u %x %X %c %s %%`, `%q` for Q16.16, a width,
and the `-` and `0` flags. The `fu_parse` functions take a pointer and a
length, with no NUL needed, and return a status instead of setting
`errno`.

## What you can rely on

`fu_fmt` prints the same digits as the C library's `%.*f`, checked on all
2^32 Q16.16 values and floats, and its integers and hex on all 2^32 inputs.
`fu_parse` reads back everything `fu_fmt` writes, rounds Q16.16 correctly,
and reports errors as an `FU_ParseStatus`. Every CRC matches its published
check value and a bitwise reference. `fu_xoshiro128_below` and
`fu_xorshift32_below` draw in a range with no bias.

## Use

In a [flex](https://github.com/antonykasera/STM32F1-HAL) project (v1.1.0 or
later), add one line to the Makefile:

```make
FLEXLIB_LIBS := utils
```

Elsewhere, include the fragment. It only sets variables:

```make
include lib/flexlib/flexutils.mk
CFLAGS += $(FLEXLIB_CFLAGS)
SRCS   += $(FU_SRCS)
```

Build with `-ffunction-sections -fdata-sections` and link with
`--gc-sections`. The functions you don't call then stay out of the image.

## Options

- `FU_CRC_SMALL`: 16-entry CRC tables instead of byte tables. Warm, CRC-32
  over 256 B then takes 4109 cycles instead of 1584. Cold, with the tables
  out of the flash cache, it takes 4672 instead of 14793: for CRCs you
  run rarely, the small tables win.
- `FX_ASSERT_FAIL(file, line)` / `FX_DISABLE_ASSERT`: by default a failed
  argument check traps, with no stdio.

## Testing

See [docs/testing.md](../docs/testing.md).
