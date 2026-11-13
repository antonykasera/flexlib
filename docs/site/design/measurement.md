# Measurement

The module pages give two kinds of numbers: cycle counts and errors. Cycle
counts are measured on the target board. Errors are measured on the host.
Both are written to generated files, and the pages read their numbers from
these files:

- `docs/results/f103.md`, written by `make bench-run`
- `docs/accuracy.md`, written by `make accuracy`

This page describes how each is measured, and what the numbers do and do not
include.

## Terms

Cycle
: One clock period of the core. At 72 MHz one cycle is about 13.9 ns.

DWT cycle counter
: A free-running 32-bit counter in the Cortex-M3 debug unit
  (`DWT->CYCCNT`). It counts core cycles.

Median
: The middle value of the samples sorted by time. A single slow sample does
  not move it.

Flash cache
: A cache between the core and the flash memory. A read that hits the cache
  takes as long as a read from SRAM. A read that misses waits for the flash.

ULP
: Unit in the last place: the gap between a float and the next float of
  the same sign.

## Cycle counts

The bench firmware runs on an STM32F103 at 72 MHz, with 2 flash wait states
and the prefetch buffer on. Each case is measured as follows:

1. The case runs once without timing.
2. Interrupts are disabled.
3. The function is called once for each of 256 inputs. The inputs come from a
   generator with a fixed seed.
4. Each call is timed by reading the cycle counter before and after it. The
   cost of the two reads is subtracted.
5. The minimum, median and maximum are recorded. The module pages show the
   median.

The firmware is built with `-O2` and newlib-nano, without link-time
optimisation. A library function is therefore timed as a real call. Rows
marked "(inline)" time the function inlined into the timing loop.

Compiler barriers keep the input after the first counter read and the
result before the second. `make bench-windows` lists the instructions between
the two reads for every case. A window that contains any instruction reads
one cycle high. The `empty, int16 arg` row times a single one-cycle
instruction and reads {cy}`empty, int16 arg`.

Each case also computes a hash of its outputs. The firmware compares it with
the hash of the same code run on the host (`make golden`). The timed code
therefore computes the same results as the code the tests check.

A call row can move by up to 2 cycles between two builds of the firmware,
because code placement changes.

### Baselines

- CMSIS-DSP is built from source with the same compiler flags. Its block
  functions are built with `ARM_MATH_LOOPUNROLL`. On the Cortex-M3 it runs
  its generic C code.
- newlib's libm and libgcc are the toolchain's prebuilt libraries.

## Warm and cold calls

The board is a clone with CPUID 0x412FC230. Its flash sits behind a cache
with 32-byte lines and a capacity between 16 and 32 KB. A miss costs about
113 cycles. The "Flash read cost" table in the results file measures this. A
genuine STM32F103 has no such cache, and its timings differ, most for tables
and for code that runs rarely.

The module pages show warm numbers. The untimed first call loads the
function's code and tables into the cache.

Cold rows flush the cache before each sample by reading 32 KB through it.
The function's code and tables then come from flash:

| function | cycles | cold, cycles |
|---|--:|--:|
| `fu_crc32`, 256 B | {cy}`fu_crc32, 256 B` | {cy}`fu_crc32, 256 B @ cold` |
| `fu_crc32`, 256 B, `FU_CRC_SMALL` | {cy}`crc32 nibble table (small), 256 B` | {cy}`crc32 nibble table (small), 256 B @ cold` |
| `fm_sin_q15` | {cy}`fm_sin_q15` | {cy}`fm_sin_q15 @ cold` |
| `arm_sin_q15` | {cy}`arm_sin_q15` | {cy}`arm_sin_q15 @ cold` |
| `fm_sinf` | {cy}`fm_sinf` | {cy}`fm_sinf @ cold` |
| newlib `sinf` | {cy}`sinf` | {cy}`sinf @ cold` |

### Choosing for cold calls

A function that is called rarely runs cold. Its table size then matters more
than its warm cycle count. `fu_crc32` reads 4 KB of tables. With
`FU_CRC_SMALL` defined, it reads 64 B. The table above shows the order of the
two reversed between warm and cold. `FM_LUT_IN_RAM` places the flexmath
tables in SRAM, which the flash cache does not affect.

## Errors

Errors are measured on the host. The library and CMSIS-DSP are compiled from
the same sources as on the bench. CMSIS-DSP uses its default flags, without
`ARM_MATH_ROUNDING`.

The reference is computed in `double`. Sums of products use an exact 128-bit
integer reference instead.

The inputs depend on the function:

- Scalar q15 functions with one argument: every input.
- Scalar q15 functions with two arguments: every pair, or a regular grid of
  pairs.
- Scalar q31, Q16.16 and float functions: $10^6$ to $10^7$ seeded inputs,
  plus edge cases.
- Block functions: $10^5$ to $10^6$ blocks or samples.

`make exhaustive` runs most scalar functions over all $2^32$ inputs: every
32-bit argument, or every pair of q15 arguments.

Errors are given in LSB of the function's own output format, or in ULP for
float results. The bias is the mean signed error.

Each header states the error bound of its functions. The tests fail if a
result exceeds it. The measured maxima on the module pages are at or below
the bound.
