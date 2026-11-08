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
| fx_ctz | flexlib | 3 | 3 | 113 | ok |
| fx_rbit | flexlib | 2 | 2 | 112 | ok |
| fx_rev | flexlib | 2 | 2 | 2 | ok |
| FX_SSAT(x, 16) | flexlib | 2 | 2 | 2 | ok |
| FX_USAT(x, 8) | flexlib | 2 | 2 | 2 | ok |
| fx_smull_hi | flexlib | 6 | 6 | 116 | ok |
| fx_umull_hi | flexlib | 6 | 6 | 6 | ok |
| sinf | libm | 703 | 1349 | 3701 | - |
| cosf | libm | 846 | 1219 | 1875 | - |
| sqrtf | libm | 595 | 595 | 2384 | - |
| atan2f | libm | 1130 | 1839 | 4584 | - |
| float multiply | libgcc | 40 | 40 | 40 | - |
| float divide | libgcc | 151 | 151 | 151 | - |
| arm_sin_q15 | CMSIS-DSP | 29 | 29 | 479 | - |
| arm_sin_f32 | CMSIS-DSP | 514 | 574 | 1358 | - |

golden: 9 ok, 0 FAIL, 0 missing
<!-- bench end -->
