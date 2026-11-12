# flexutils

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexutils-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

Everyday Cortex-M code, fast and checked on every input it can take: bit
tricks, formatting and parsing without printf, CRCs, a lock-free ring, COBS,
debouncing and PRNGs. Plain C11, no allocation.

Cycles are medians on an STM32F103 at 72 MHz (`-O2`, warm cache), from
[docs/results/f103.md](../docs/results/f103.md).

## Modules

| header | what | cycles | baseline |
|---|---|--:|---|
| `fu_bits.h` | clz / ctz (32, 64), popcount, rbit, byte swaps, LE / BE loads and stores, sign extension of N-bit fields | popcount 13, clz64 3, `fu_load_be32` 4, `fu_sext(x, 12)` 2 | `__builtin_popcount` 17 |
| `fu_fmt.h` | u32, i32, hex, Q16.16 and float to text; `fu_snprintf` (`%d %i %u %x %X %c %s %%`, `%q` for Q16.16; width, `-`, `0`) | u32 116 (10 digits), Q16.16 `%.4f` 154, float `%.2f` 182 | `snprintf` 1024, 4281, 4285 |
| `fu_parse.h` | text to u32, i32, hex, Q16.16 (correctly rounded); bounded input, status return | u32 153 (10 digits), Q16.16 275 | `strtoul` 327, `strtod` path 1332 |
| `fu_crc.h` | CRC-8 SMBus / Maxim, CRC-16 CCITT / MODBUS, CRC-32 (zlib) | CRC-32 6.2 per byte (256 B: 1584), CRC-16 8.9, CRC-8 7.1 | byte table 11, bitwise 54 per byte |
| `fu_ring.h` | lock-free SPSC byte ring, fixed-size element queue, circular-DMA receive | write + read 64 B: 392; put + get 33 | memcpy-based 933 |
| `fu_cobs.h` | COBS encode, strict in-place decode, byte-at-a-time receiver | encode 64 B 329, decode 342 | byte loop 996, 906 |
| `fu_debounce.h` | integrator for one input; branch-free vertical counter for 32 | 32 inputs per tick: 20 | 16 single integrators 379 |
| `fu_rand.h` | xorshift32, xoshiro128** with jump, unbiased bounded draws | xoshiro128** 22, unbiased `below(100)` 33 | `rand()` 44 |

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

## Checking it

`make` runs the host tests, the cross builds and the asm audit;
`make exhaustive` and `make exhaustive-slow` cover every 32-bit input;
`make bench-run` measures on the board. See the
[top-level README](../README.md).
