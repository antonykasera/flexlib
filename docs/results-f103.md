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
| sinf | libm | 703 | 1347 | 1460 | - |
| cosf | libm | 846 | 1219 | 1596 | - |
| sqrtf | libm | 595 | 595 | 595 | - |
| atan2f | libm | 1130 | 1839 | 2005 | - |
| float multiply | libgcc | 40 | 40 | 40 | - |
| float divide | libgcc | 151 | 151 | 151 | - |
| arm_sin_q15 | CMSIS-DSP | 29 | 29 | 29 | - |
| arm_sin_f32 | CMSIS-DSP | 401 | 572 | 583 | - |

golden: 9 ok, 0 FAIL, 0 missing

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
