# flexutils

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexutils-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

Small building blocks for Cortex-M: bit operations, text formatting and
parsing without printf, CRCs, a lock-free ring buffer, COBS, debouncing and
random numbers. Plain C11, no allocation.

Cycles are medians on an STM32F103 at 72 MHz (`-O2`, warm cache), from
[docs/results/f103.md](../docs/results/f103.md).

## Modules

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
`-` and `0`. `fu_parse` takes a length and returns a status.

## Guarantees

- `fu_fmt` matches the C library's `%.*f` exactly for all 2^32 Q16.16 values
  and floats; integers and hex are checked on all 2^32 inputs.
- `fu_parse` reads back everything `fu_fmt` writes and rounds Q16.16
  correctly; errors come back as `FU_ParseStatus`.
- CRCs match the published check values and a bitwise reference.
- `fu_xoshiro128_below` and `fu_xorshift32_below` are unbiased.

## Use

In a [flex](https://github.com/antonykasera/STM32F1-HAL) project (v1.1.0 or
later), one line in the Makefile:

```make
FLEXLIB_LIBS := utils
```

Elsewhere, include the fragment (it only sets variables):

```make
include lib/flexlib/flexutils.mk
CFLAGS += $(FLEXLIB_CFLAGS)
SRCS   += $(FU_SRCS)
```

```c
#include "fu_fmt.h"
#include "fu_ring.h"

static uint8_t rx_mem[256];
static FU_Ring rx;

void app_init(void) { fu_ring_init(&rx, rx_mem, sizeof rx_mem); }

void report(char *buf, size_t n, int32_t mv, int32_t temp_q16) {
  fu_snprintf(buf, n, "v=%d mV t=%.2q C\n", mv, temp_q16); /* no printf */
}
```

Build with `-ffunction-sections -fdata-sections` and link with
`--gc-sections`, so unused functions drop out.

## Options

- `FU_CRC_SMALL`: 16-entry CRC tables instead of byte tables. CRC-32 over
  256 B takes 4109 cycles instead of 1584 warm, and 4672 instead of 14793 cold.
- `FX_ASSERT_FAIL(file, line)` / `FX_DISABLE_ASSERT`: argument checks trap by
  default, with no stdio.

## Testing

See [docs/testing.md](../docs/testing.md).
