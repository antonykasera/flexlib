# Bench results: STM32F103C8 (Blue Pill clone)

Written by `make bench-run` on 2026-11-08. Do not edit by hand;
rerun it. Method: the Bench section of README.md.

<!-- bench begin -->
## flexlib bench, STM32F103 @ 72 MHz

- build: -O2, GCC 16.2.0, newlib-nano
- CPUID 0x412FC230, flash 2 WS, prefetch on
- cycles per call over 256 inputs, CYCCNT read cost (1) subtracted; interrupts off while timing

| case | impl | min | median | max | golden |
|---|---|--:|--:|--:|---|
| CYCCNT read (raw) | - | 1 | 1 | 1 | - |
| empty, int16 arg | - | 2 | 2 | 2 | ok |
| fx_clz | flexlib | 2 | 2 | 2 | ok |
| fx_ctz | flexlib | 3 | 3 | 3 | ok |
| fx_rbit | flexlib | 2 | 2 | 2 | ok |
| fx_rev | flexlib | 2 | 2 | 2 | ok |
| FX_SSAT(x, 16) | flexlib | 2 | 2 | 2 | ok |
| FX_USAT(x, 8) | flexlib | 2 | 2 | 2 | ok |
| fx_smull_hi | flexlib | 6 | 6 | 6 | ok |
| fx_umull_hi | flexlib | 6 | 6 | 6 | ok |
| fu_popcount | flexlib | 13 | 13 | 13 | ok |
| popcount SWAR, call | flexlib | 17 | 17 | 17 | ok |
| popcount __builtin | libgcc | 17 | 17 | 17 | ok |
| popcount table 256 B, call | candidate | 23 | 23 | 23 | ok |
| popcount table 16 B, call | candidate | 89 | 89 | 89 | ok |
| popcount x &= x - 1, call | candidate | 52 | 87 | 127 | ok |
| fu_clz64, half hi = 0 | flexlib | 3 | 9 | 9 | ok |
| fu_clz64, hi != 0 | flexlib | 3 | 3 | 3 | ok |
| clz64 branchless, half hi = 0 | candidate | 6 | 6 | 6 | ok |
| clz64 branchless, hi != 0 | candidate | 6 | 6 | 6 | ok |
| fu_ctz64, half lo = 0 | flexlib | 4 | 11 | 11 | ok |
| fu_ctz64, lo != 0 | flexlib | 4 | 4 | 4 | ok |
| ctz64 branchless, half lo = 0 | candidate | 9 | 9 | 9 | ok |
| ctz64 branchless, lo != 0 | candidate | 8 | 8 | 8 | ok |
| fu_load_be32, aligned | flexlib | 4 | 4 | 4 | ok |
| fu_load_be32, +1 | flexlib | 6 | 6 | 6 | ok |
| fu_load_be32, +2 | flexlib | 5 | 5 | 5 | ok |
| fu_sext(x, 12) | flexlib | 2 | 2 | 2 | ok |
| fu_sext(x, n) | flexlib | 7 | 7 | 7 | ok |
| sinf | libm | 704 | 1348 | 1461 | - |
| cosf | libm | 847 | 1220 | 1597 | - |
| sqrtf | libm | 596 | 596 | 596 | - |
| atan2f | libm | 1130 | 1839 | 2005 | - |
| float multiply | libgcc | 40 | 40 | 40 | - |
| float divide | libgcc | 149 | 149 | 149 | - |
| arm_sin_q15 | CMSIS-DSP | 29 | 29 | 29 | - |
| arm_sin_f32 | CMSIS-DSP | 402 | 573 | 584 | - |

golden: 28 ok, 0 FAIL, 0 missing

### Flash read cost

Cycles per 32-bit load from flash, loop included, first pass after a flush (cold) and second pass (warm). The same loop over SRAM: 8.3.

| block, one load per 32 B | cold | warm |
|--:|--:|--:|
| 256 B | 122.1 | 9.1 |
| 512 B | 121.5 | 8.5 |
| 1 KB | 121.2 | 8.2 |
| 2 KB | 121.1 | 8.1 |
| 4 KB | 121.0 | 8.0 |
| 8 KB | 121.0 | 8.0 |
| 16 KB | 121.0 | 8.0 |
| 32 KB | 121.0 | 121.0 |

| stride, 1 KB block | cold | warm |
|--:|--:|--:|
| 4 B | 22.1 | 8.0 |
| 8 B | 36.3 | 8.0 |
| 16 B | 64.6 | 8.1 |
| 32 B | 121.2 | 8.2 |
| 64 B | 121.5 | 8.5 |
<!-- bench end -->
