# flexutils

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexutils-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

Bit and byte operations, number formatting and parsing, CRCs, ring buffers,
COBS framing, debouncing and random numbers for Cortex-M. Written in C11 with
no allocation and no dependency on `printf`.

## Example

```c
#include <stdio.h>

#include "fu_fmt.h"

int main(void) {
  int32_t temp = 1546650; /* 23.6 in Q16.16 */
  char line[48];
  fu_snprintf(line, sizeof line, "t=%.1q C id=%08X n=%u", temp, 0xBEEFU, 42U);
  puts(line);
  return 0;
}
```

Output:

```text
t=23.6 C id=0000BEEF n=42
```

`%q` formats a Q16.16 value with the digits `%f` gives for its exact value.

## Modules

| Header | Contents | Cycles | Baseline |
| --- | --- | --: | --- |
| `fu_bits.h` | clz, ctz, popcount, bit reverse, byte swaps, LE / BE loads, sign extension | popcount 13 | `__builtin_popcount` 17 |
| `fu_fmt.h` | Integers, hex, Q16.16 and float to text, `fu_snprintf` | u32 116, float `%.2f` 182 | `snprintf` 1024, 4285 |
| `fu_parse.h` | Text to u32, i32, hex, Q16.16 | u32 153, Q16.16 275 | `strtoul` 327, `strtod` 1332 |
| `fu_crc.h` | CRC-8 SMBus / Maxim, CRC-16 CCITT / MODBUS, CRC-32 | CRC-32 6.2 per byte | bitwise 54 per byte |
| `fu_ring.h` | Lock-free byte ring, element queue, DMA receive | 64 B write + read 392 | `memcpy` ring 933 |
| `fu_cobs.h` | COBS encode, in-place decode, byte receiver | 64 B encode 329 | byte loop 996 |
| `fu_debounce.h` | One input, or 32 inputs in parallel | 32 inputs 20 per tick | 16 integrators 379 |
| `fu_rand.h` | xorshift32, xoshiro128**, unbiased bounded draws | xoshiro128** 22 | `rand()` 44 |

Cycles are medians on an STM32F103 at 72 MHz (`-O2`, warm cache). Full results
are in [docs/results/f103.md](../docs/results/f103.md).

`fu_snprintf` supports `%d %i %u %x %X %c %s %%`, `%q` for Q16.16, a width,
and the `-` and `0` flags. The `fu_parse` functions take a pointer and a
length, do not require NUL termination, and return a status instead of
setting `errno`.

## Accuracy

| Module | Verification |
| --- | --- |
| `fu_fmt` | Same digits as the C library's `%.*f` on all 2^32 Q16.16 values and floats, and integers and hex on all 2^32 inputs |
| `fu_parse` | Reads back `fu_fmt` output, with Q16.16 correctly rounded |
| `fu_crc` | Published check values and a bitwise reference |
| `fu_rand` | Bounded draws without bias |

## Configuration

| Macro | Effect |
| --- | --- |
| `FU_CRC_SMALL` | 16-entry CRC tables instead of 256-entry tables. CRC-32 over 256 B takes 4109 cycles instead of 1584 warm, and 4672 instead of 14793 with the tables out of the flash cache. |

## Usage

Select the library with `FLEXLIB_LIBS := utils`, or link `flexlib::utils` in
CMake. Build integration is described in the [root README](../README.md#usage).

## Testing

See [docs/testing.md](../docs/testing.md).
