# Bench results: STM32F103C8 (Blue Pill clone)

Written by `make bench-run` on 2026-11-09. Do not edit by hand;
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
| fu_fmt_u32, 10 digits | flexlib | 116 | 116 | 116 | ok |
| fu_fmt_u32, log | flexlib | 46 | 83 | 120 | ok |
| u32 split at 1e8, no branch, 10 digits | candidate | 123 | 124 | 124 | ok |
| u32 split at 1e8, no branch, log | candidate | 121 | 126 | 129 | ok |
| u32 fixed point unrolled, 10 digits | candidate | 117 | 117 | 117 | ok |
| u32 fixed point unrolled, log | candidate | 40 | 98 | 122 | ok |
| u32 by 10s, 10 digits | candidate | 173 | 173 | 173 | ok |
| u32 by 10s, log | candidate | 50 | 103 | 173 | ok |
| u32 by 10s UDIV, 10 digits | candidate | 185 | 187 | 190 | ok |
| u32 by 10s UDIV, log | candidate | 50 | 105 | 190 | ok |
| snprintf %lu, 10 digits | newlib | 1022 | 1024 | 1027 | ok |
| snprintf %lu, log | newlib | 788 | 887 | 1027 | ok |
| fu_fmt_i32, log | flexlib | 56 | 93 | 130 | ok |
| fu_fmt_hex32, 8 digits | flexlib | 78 | 78 | 78 | ok |
| fu_fmt_hex32, minimal | flexlib | 56 | 56 | 78 | ok |
| hex nibble loop, 8 digits | candidate | 103 | 103 | 103 | ok |
| hex nibble loop, minimal | candidate | 40 | 67 | 103 | ok |
| fu_fmt_dec_digits | flexlib | 20 | 20 | 20 | ok |
| fu_fmt_q16, %.4f, log | flexlib | 151 | 154 | 190 | ok |
| snprintf %.4f of q16, log | newlib | 1837 | 4282 | 6350 | ok |
| fu_fmt_f32, %.2f, 2^-7..2^13 | flexlib | 178 | 182 | 203 | ok |
| snprintf %.2f, 2^-7..2^13 | newlib | 3230 | 4286 | 5449 | ok |
| fu_fmt_f32, %.0f, >= 2^32 | flexlib | 334 | 746 | 1485 | ok |
| snprintf %.0f, >= 2^32 | newlib | 5963 | 9700 | 17071 | ok |
| fu_snprintf u32 + i32 + hex line | flexlib | 705 | 763 | 838 | ok |
| fu_snprintf v1 (tmp + memcpy), same line | candidate | 941 | 1039 | 1213 | ok |
| snprintf u32 + i32 + hex line | newlib | 3232 | 3279 | 3334 | ok |
| fu_crc32, 256 B | flexlib | 1457 | 1585 | 1585 | ok |
| crc32 byte table 1 KB, 256 B | candidate | 2828 | 2828 | 2828 | ok |
| crc32 nibble table (small), 256 B | candidate | 4108 | 4108 | 4108 | ok |
| crc32 bitwise, 256 B | candidate | 13842 | 13842 | 13842 | ok |
| crc32 F1 CRC unit + RBIT, 256 B | hardware | 596 | 724 | 724 | ok |
| crc32 slice-by-4, aligned head, 256 B | candidate | 1523 | 1544 | 1546 | ok |
| fu_crc32, 8 B | flexlib | 93 | 97 | 97 | ok |
| crc32 byte table 1 KB, 8 B | candidate | 101 | 101 | 101 | ok |
| crc32 nibble table (small), 8 B | candidate | 141 | 141 | 141 | ok |
| crc32 slice-by-4, aligned head, 8 B | candidate | 98 | 119 | 121 | ok |
| fu_crc16_ccitt, 64 B | flexlib | 552 | 568 | 568 | ok |
| crc16 ccitt nibble table (small), 64 B | candidate | 1168 | 1168 | 1168 | ok |
| crc16 ccitt bitwise, 64 B | candidate | 3984 | 3984 | 3984 | ok |
| crc16 ccitt byte table, 1 per step, 64 B | candidate | 716 | 716 | 716 | ok |
| fu_crc16_modbus, 64 B | flexlib | 506 | 523 | 525 | ok |
| crc16 modbus nibble table (small), 64 B | candidate | 1041 | 1041 | 1041 | ok |
| crc16 modbus bitwise, 64 B | candidate | 3472 | 3472 | 3472 | ok |
| crc16 modbus byte table, 1 per step, 64 B | candidate | 779 | 779 | 779 | ok |
| fu_crc8_smbus, 64 B | flexlib | 441 | 453 | 455 | ok |
| crc8 smbus nibble table (small), 64 B | candidate | 1099 | 1099 | 1099 | ok |
| crc8 smbus bitwise, 64 B | candidate | 3466 | 3466 | 3466 | ok |
| crc8 smbus byte table, 1 per step, 64 B | candidate | 524 | 524 | 524 | ok |
| fu_crc8_maxim, 64 B | flexlib | 441 | 453 | 455 | ok |
| crc8 maxim nibble table (small), 64 B | candidate | 1104 | 1104 | 1104 | ok |
| crc8 maxim bitwise, 64 B | candidate | 3466 | 3466 | 3466 | ok |
| crc8 maxim byte table, 1 per step, 64 B | candidate | 524 | 524 | 524 | ok |
| fu_ring write + read, 64 B | flexlib | 322 | 390 | 528 | ok |
| ring, word copy not unrolled, 64 B | candidate | 359 | 423 | 478 | ok |
| ring, newlib memcpy, 64 B | candidate | 931 | 931 | 972 | ok |
| ring, put + get per byte, 64 B | candidate | 2589 | 2589 | 2589 | ok |
| fu_ring write + read, 8 B | flexlib | 197 | 207 | 258 | ok |
| ring, newlib memcpy, 8 B | candidate | 259 | 259 | 300 | ok |
| ring, put + get per byte, 8 B | candidate | 349 | 349 | 349 | ok |
| fu_ring_put + fu_ring_get (inline) | flexlib | 33 | 33 | 33 | ok |
| fu_queue push + pop, 12 B | flexlib | 166 | 166 | 166 | ok |
| fu_ring_from_dma + read, 64 B | flexlib | 380 | 480 | 659 | ok |
| fu_parse_u32, log | flexlib | 80 | 111 | 152 | ok |
| u32 one digit per step, log | candidate | 63 | 115 | 180 | ok |
| strtoul, log | newlib | 129 | 217 | 327 | ok |
| fu_parse_u32, 10 digits | flexlib | 152 | 152 | 152 | ok |
| u32 one digit per step, 10 digits | candidate | 180 | 180 | 180 | ok |
| strtoul, 10 digits | newlib | 327 | 327 | 327 | ok |
| fu_parse_i32, log | flexlib | 100 | 133 | 174 | ok |
| strtol, log | newlib | 136 | 228 | 345 | ok |
| fu_parse_hex32, 8 digits | flexlib | 195 | 195 | 200 | ok |
| hex two-branch classify, 8 digits | candidate | 204 | 219 | 239 | ok |
| strtoul base 16, 8 digits | newlib | 290 | 314 | 356 | ok |
| fu_parse_q16, 4 decimals | flexlib | 247 | 275 | 316 | ok |
| strtod * 65536 + lrint, 4 decimals | newlib | 786 | 1332 | 1413 | ok |
| sinf | libm | 704 | 1348 | 1461 | - |
| cosf | libm | 847 | 1220 | 1597 | - |
| sqrtf | libm | 596 | 596 | 596 | - |
| atan2f | libm | 1130 | 1839 | 2005 | - |
| float multiply | libgcc | 40 | 40 | 40 | - |
| float divide | libgcc | 149 | 149 | 149 | - |
| arm_sin_q15 | CMSIS-DSP | 29 | 29 | 29 | - |
| arm_sin_f32 | CMSIS-DSP | 402 | 573 | 584 | - |

golden: 104 ok, 0 FAIL, 0 missing

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
