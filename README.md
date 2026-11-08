# flexlib

Compute libraries for Cortex-M, measured on the chip they're for:

- **flexutils** (`fu_`): bit tricks, lock-free rings, printf-free formatting
  and parsing, CRCs, COBS, debouncing, PRNGs.
- **flexmath** (`fm_`): fixed point (Q15 / Q31 / Q16.16), and fast float
  functions built on integer internals, for chips with no FPU.

The first target is the STM32F103 (Cortex-M3, 72 MHz, no FPU). Each function
states an error bound, which the host tests prove against a `double`
reference, and has a cycle count measured on the board next to newlib libm and
CMSIS-DSP. flexlib is plain C11 with no dependencies. The bench firmware runs
on [flex](https://github.com/antonykasera/STM32F1-HAL).

## Status

Early: the arch layer, test harness and bench firmware are in place, and
flexutils is under way.

| Part | Host | Board |
|---|---|---|
| `arch/fx_arch.h` (CLZ, RBIT, REV, SSAT/USAT, SMULL/UMULL/SMLAL, barriers) | tests + asm-audit | golden check passes |
| bench firmware (DWT cycles, USB CDC output, golden check) | builds | runs |
| `fu_bits` (clz/ctz 32/64, popcount, bswap, endian loads/stores, sign extension) | exhaustive + random tests, asm-audit | bench + golden check |
| `fu_fmt` integers (u32, i32, hex) | all 2^32 inputs (u32, hex), tests against snprintf, asm-audit | bench + golden check |
| `fu_fmt` Q16.16 and float (printf's `%.*f`, exact, no soft-float), `fu_snprintf` | all 2^32 inputs (q16, f32) against the C library's `%.*f`, tests against snprintf, asm-audit | bench + golden check |
| `fu_crc` (CRC-8 SMBus/Maxim, CRC-16 CCITT/MODBUS, CRC-32 zlib; `FU_CRC_SMALL` 16-entry tables) | check values, random data against a bitwise reference (both table sizes), asm-audit | bench + golden check |
| `fu_ring` (lock-free SPSC byte ring, fixed-size element queue, circular-DMA receive) | random operations against a reference FIFO (indices across the 2^32 wrap), DMA simulation, asm-audit | bench + golden check |

## Layout

```
include/      public headers (fu_*.h, fm_*.h)
arch/         fx_arch.h: single-instruction wrappers, plain C on the host
utils/src/    flexutils sources
math/src/     flexmath sources
test/host/    host tests: harness, error statistics, ULP error
test/asm/     asm-audit spec and probes
bench/        F103 bench firmware (flex and CMSIS-DSP as submodules)
tools/        asm_audit.py, bench_run.py, bench_windows.py, gen_crc_tables.py
docs/         results-f103.md
```

## Using it

```make
include lib/flexlib/flexmath.mk     # or flexutils.mk on its own
CFLAGS += $(FLEXLIB_CFLAGS)
SRCS   += $(FM_SRCS) $(FU_SRCS)
```

Build with `-ffunction-sections -fdata-sections` and link with
`--gc-sections`, so the functions you don't call drop out.

Argument checks (`FX_ASSERT`) trap on failure, with no stdio. Define
`FX_ASSERT_FAIL(file, line)` to call your own handler instead, or
`FX_DISABLE_ASSERT` to compile the checks out.

## Building and testing

| Command | What it does |
|---|---|
| `make` | `host-test`, `cross`, `lint`, `asm-audit` |
| `make host-test [HOST_CC=clang]` | host tests with ASan + UBSan |
| `make exhaustive` | checks functions of one 32-bit input on every input (~1 min on 12 cores) |
| `make exhaustive-slow` | the same for q16 and float text, against the C library's `%.*f` (~13 min on 12 cores) |
| `make cross` | `-Werror` builds for Cortex-M3, M4, M7 |
| `make lint` | `-Wpedantic` everywhere, each header compiled on its own |
| `make asm-audit` | checks the M3 disassembly for the expected instructions and for no soft-float or libgcc calls |
| `make tables` | regenerates the CRC tables (`utils/src/fu_crc_tab.c`) |
| `make golden` | runs the bench cases on the host and writes `bench/golden.c` |
| `make bench [BENCH_OPT=-Os]` | builds the bench firmware |
| `make bench-run` | flashes it over ST-Link, reads `/dev/ttyACM0`, writes `docs/results-f103.md` |
| `make bench-windows` | lists the instructions each bench case actually times |

The bench needs the submodules: `git submodule update --init`.

## Bench

- Each case times one call per input with the DWT cycle counter, over 256
  fixed pseudo-random inputs, and reports min, median and max. Interrupts are
  off while it times, and USB is polled. The cost of reading the counter is
  measured and subtracted.
- Each case first runs once untimed, so the numbers are warm. The bench board
  is an F103 clone (CPUID 0x412FC230) whose flash behaves like a cache. The
  probe below measures 32-byte lines, a capacity between 16 and 32 KB, about
  113 cycles (1.6 us) per cold line with no next-line prefetch, and hits as
  fast as SRAM. A genuine STM32F103 has no cache (2 wait states plus a
  prefetch buffer), so its timings may differ, especially for table lookups
  and for code that hasn't run recently.
- Built at `-O2` without LTO, so library functions are timed as real calls.
  libm is prebuilt newlib, so its numbers don't depend on the bench's flags.
- `make bench-windows` lists the exact instructions between the two counter
  reads of every case.
- The same case code also runs on the host. The board compares its output
  hash for each case against the host's, which catches any difference between
  the two builds.
- Any non-empty window reads one cycle high, because the second counter read
  no longer pipelines with the first. A single-instruction case shows 2. With
  GCC, a 16-bit argument costs one extra SXTH. The `empty, int16 arg` row
  measures both effects.
- Samples above 2 x median + 4 are counted per case and listed by input
  index under the table, so data-dependent timing stands out.
- A "Flash read cost" section measures the board's flash: cycles per load,
  cold and warm, by block size and by stride. The loop runs from SRAM so its
  own fetches don't disturb the result.
- The firmware needs 128 KB of flash: an F103CB, or a C8 that has it, as
  most clones do (`st-info --probe` shows the size).

## Results

[docs/results-f103.md](docs/results-f103.md): every bench case, with the
newlib, libm and CMSIS-DSP baselines and the candidates each function was
chosen from.

## License

MIT, see [LICENSE](LICENSE). CMSIS-DSP (bench baseline only) is Apache-2.0.
