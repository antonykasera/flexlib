# Measurement

The module pages give two kinds of numbers: how many cycles a function
takes, and how far its results are from the exact answer. Cycles are
measured on the board, and errors on the host. Both go into generated files,
and the pages read every number from those files, so nothing on the site is
typed by hand:

- `docs/results/f103.md`, written by `make bench-run`
- `docs/accuracy.md`, written by `make accuracy`

This page explains how each is measured, and what the numbers do and do not
include, so you can judge how they apply to your own code.

## Counting cycles

A cycle is one clock period of the core, about 13.9 ns at 72 MHz. The
Cortex-M3 has a free-running 32-bit counter in its debug unit,
`DWT->CYCCNT`, that counts them. The bench firmware runs on an STM32F103 at
72 MHz, with 2 flash wait states and the prefetch buffer on, and measures
each case like this:

1. The case runs once without timing, which loads its code and tables into
   the flash cache.
2. Interrupts are disabled.
3. The function is called once for each of 256 inputs. The inputs come from
   a generator with a fixed seed, so every run times the same work.
4. Each call is timed by reading the counter before and after it, and the
   cost of the two reads is subtracted.
5. The minimum, median and maximum are recorded. The module pages show the
   median: the middle of the sorted times, which one slow sample does not
   move.

The firmware is built with `-O2` and newlib-nano, without link-time
optimisation, so a library function is timed as a real call, as your code
would make it. Rows marked "(inline)" time the function inlined into the
timing loop.

Compiler barriers keep the input after the first counter read and the
result before the second. `make bench-windows` lists the instructions
between the two reads for every case. A window that contains any
instruction reads one cycle high. The `empty, int16 arg` row times a single
one-cycle instruction, and reads {cy}`empty, int16 arg`.

Each case also hashes its outputs, and the firmware compares the hash with
the same code run on the host (`make golden`). The code being timed
therefore computes the same results as the code the tests check, not a
version the compiler optimised away.

A call can move by up to 2 cycles between two builds of the firmware,
because the code lands at different addresses.

### Baselines

CMSIS-DSP is built from source with the same compiler flags, and its block
functions with `ARM_MATH_LOOPUNROLL`. On the Cortex-M3 it runs its generic C
code. newlib's libm and libgcc are the toolchain's prebuilt libraries.

## Warm and cold calls

The board is a clone with CPUID 0x412FC230. Its flash sits behind a cache
with 32-byte lines and a capacity between 16 and 32 KB. A read that hits the
cache is as fast as SRAM. A miss waits for the flash, about 113 cycles. The
"Flash read cost" table in the results file measures this. A genuine
STM32F103 has no such cache, and its timings differ, most for tables and
for code that runs rarely.

The module pages show warm numbers: the untimed first call has already
loaded the function's code and tables into the cache. Cold rows flush the
cache before each sample by reading 32 KB through it, so the function's code
and tables come from flash:

| function | cycles | cold, cycles |
|---|--:|--:|
| `fu_crc32`, 256 B | {cy}`fu_crc32, 256 B` | {cy}`fu_crc32, 256 B @ cold` |
| `fu_crc32`, 256 B, `FU_CRC_SMALL` | {cy}`crc32 nibble table (small), 256 B` | {cy}`crc32 nibble table (small), 256 B @ cold` |
| `fm_sin_q15` | {cy}`fm_sin_q15` | {cy}`fm_sin_q15 @ cold` |
| `arm_sin_q15` | {cy}`arm_sin_q15` | {cy}`arm_sin_q15 @ cold` |
| `fm_sinf` | {cy}`fm_sinf` | {cy}`fm_sinf @ cold` |
| newlib `sinf` | {cy}`sinf` | {cy}`sinf @ cold` |

A function you call rarely runs cold, and then its table size matters more
than its warm cycle count. `fu_crc32` reads 4 KB of tables, and with
`FU_CRC_SMALL` defined only 64 B. As the table shows, the faster of the two
warm is the slower one cold. `FM_LUT_IN_RAM` puts flexmath's tables in SRAM,
where the flash cache does not matter.

## Errors

Errors are measured on the host. The library and CMSIS-DSP are compiled
from the same sources as on the bench. CMSIS-DSP uses its default flags,
without `ARM_MATH_ROUNDING`. The reference is computed in `double`, except
for sums of products, which use an exact 128-bit integer reference.

How many inputs are tried depends on the function:

- Scalar q15 functions of one argument: every input.
- Scalar q15 functions of two arguments: every pair, or a regular grid of
  pairs.
- Scalar q31, Q16.16 and float functions: $10^6$ to $10^7$ seeded inputs,
  plus the edge cases.
- Block functions: $10^5$ to $10^6$ blocks or samples.

`make exhaustive` goes further and runs most scalar functions over all
$2^32$ inputs: every 32-bit argument, or every pair of q15 arguments.

Errors are given in LSB of the function's own output format, or in ULP for
float results: the gap between a float and the next one of the same sign.
The bias is the mean signed error. Each header states its functions' error
bound, and the tests fail if any result passes it, so the measured maxima on
the module pages are at or below the bound.
