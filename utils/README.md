# flexutils

The small, everyday code of a Cortex-M firmware, made fast and checked on every
input it can take: bit tricks, formatting and parsing without printf or
soft-float, CRCs, a lock-free ring for interrupts and DMA, COBS framing,
debouncing and PRNGs. Plain C11, no allocation, no dependencies beyond
`arch/fx_arch.h`.

Cycles below are medians on an STM32F103 at 72 MHz (`-O2`, GCC 16, warm
cache), from [docs/results-f103.md](../docs/results-f103.md), next to newlib
or the obvious alternative in the same firmware.

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

## What is guaranteed

- `fu_fmt` gives the C library's `%.*f` text exactly, for all 2^32 Q16.16
  values and all 2^32 floats (`make exhaustive-slow`), with no soft-float and
  no 64-bit division. Integers and hex are checked on all 2^32 inputs.
- `fu_parse` reads back everything `fu_fmt` writes (all 2^32 values of each
  type, Q16.16 included), rounds Q16.16 correctly, ties included, and reports
  overflow and bad input through `FU_ParseStatus` instead of guessing.
- CRCs match the published check values and a bitwise reference on random
  data, for both table sizes.
- `fu_ring` indices run freely across the 2^32 wrap; the ring needs only
  compiler barriers on a single-core M3. `fu_ring_from_dma` turns a circular
  DMA buffer's remaining count into ring data.
- `fu_xoshiro128_below` and `fu_xorshift32_below` are unbiased (rejection,
  with UDIV only on the rare rejection). xorshift32's full period is checked.

## Use

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

- `FU_CRC_SMALL`: 16-entry CRC tables instead of byte tables (CRC-32's is
  4 KB, slice-by-4). Warm, CRC-32 over 256 B then takes 4109 cycles instead of
  1584; cold, 4672 instead of 14793, because slice-by-4 reads most of its
  table. Choose it for code that runs rarely, or for flash without a cache.
- `FX_ASSERT_FAIL(file, line)` / `FX_DISABLE_ASSERT`: argument checks trap by
  default, with no stdio.

## Checking it

`make` runs the host tests (ASan, UBSan, gcc and clang), the `-Werror` cross
builds and the asm audit; `make exhaustive` and `make exhaustive-slow` cover
every 32-bit input; `make bench-run` measures it on the board. See the
[top-level README](../README.md).
