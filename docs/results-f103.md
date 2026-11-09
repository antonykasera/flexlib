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
| fu_crc32, 256 B | flexlib | 1456 | 1584 | 1584 | ok |
| crc32 byte table 1 KB, 256 B | candidate | 2829 | 2829 | 2829 | ok |
| crc32 nibble table (small), 256 B | candidate | 4109 | 4109 | 4109 | ok |
| crc32 bitwise, 256 B | candidate | 13843 | 13843 | 13843 | ok |
| crc32 F1 CRC unit + RBIT, 256 B | hardware | 596 | 724 | 724 | ok |
| crc32 slice-by-4, aligned head, 256 B | candidate | 1524 | 1545 | 1547 | ok |
| fu_crc32, 8 B | flexlib | 93 | 97 | 97 | ok |
| crc32 byte table 1 KB, 8 B | candidate | 100 | 100 | 100 | ok |
| crc32 nibble table (small), 8 B | candidate | 140 | 140 | 140 | ok |
| crc32 slice-by-4, aligned head, 8 B | candidate | 97 | 118 | 120 | ok |
| fu_crc16_ccitt, 64 B | flexlib | 552 | 568 | 568 | ok |
| crc16 ccitt nibble table (small), 64 B | candidate | 1169 | 1169 | 1169 | ok |
| crc16 ccitt bitwise, 64 B | candidate | 3985 | 3985 | 3985 | ok |
| crc16 ccitt byte table, 1 per step, 64 B | candidate | 717 | 717 | 717 | ok |
| fu_crc16_modbus, 64 B | flexlib | 506 | 523 | 525 | ok |
| crc16 modbus nibble table (small), 64 B | candidate | 1042 | 1042 | 1042 | ok |
| crc16 modbus bitwise, 64 B | candidate | 3473 | 3473 | 3473 | ok |
| crc16 modbus byte table, 1 per step, 64 B | candidate | 780 | 780 | 780 | ok |
| fu_crc8_smbus, 64 B | flexlib | 441 | 453 | 455 | ok |
| crc8 smbus nibble table (small), 64 B | candidate | 1100 | 1100 | 1100 | ok |
| crc8 smbus bitwise, 64 B | candidate | 3467 | 3467 | 3467 | ok |
| crc8 smbus byte table, 1 per step, 64 B | candidate | 525 | 525 | 525 | ok |
| fu_crc8_maxim, 64 B | flexlib | 441 | 453 | 455 | ok |
| crc8 maxim nibble table (small), 64 B | candidate | 1105 | 1105 | 1105 | ok |
| crc8 maxim bitwise, 64 B | candidate | 3467 | 3467 | 3467 | ok |
| crc8 maxim byte table, 1 per step, 64 B | candidate | 525 | 525 | 525 | ok |
| fu_ring write + read, 64 B | flexlib | 324 | 392 | 530 | ok |
| ring, word copy not unrolled, 64 B | candidate | 361 | 425 | 480 | ok |
| ring, newlib memcpy, 64 B | candidate | 933 | 933 | 974 | ok |
| ring, put + get per byte, 64 B | candidate | 2527 | 2527 | 2527 | ok |
| fu_ring write + read, 8 B | flexlib | 199 | 209 | 260 | ok |
| ring, newlib memcpy, 8 B | candidate | 261 | 261 | 302 | ok |
| ring, put + get per byte, 8 B | candidate | 343 | 343 | 343 | ok |
| fu_ring_put + fu_ring_get (inline) | flexlib | 33 | 33 | 33 | ok |
| fu_queue push + pop, 12 B | flexlib | 166 | 166 | 166 | ok |
| fu_ring_from_dma + read, 64 B | flexlib | 380 | 480 | 659 | ok |
| fu_parse_u32, log | flexlib | 80 | 111 | 152 | ok |
| u32 one digit per step, log | candidate | 63 | 115 | 180 | ok |
| strtoul, log | newlib | 129 | 217 | 327 | ok |
| fu_parse_u32, 10 digits | flexlib | 153 | 153 | 153 | ok |
| u32 one digit per step, 10 digits | candidate | 181 | 181 | 181 | ok |
| strtoul, 10 digits | newlib | 327 | 327 | 327 | ok |
| fu_parse_i32, log | flexlib | 100 | 133 | 174 | ok |
| strtol, log | newlib | 136 | 228 | 345 | ok |
| fu_parse_hex32, 8 digits | flexlib | 196 | 196 | 201 | ok |
| hex two-branch classify, 8 digits | candidate | 205 | 220 | 240 | ok |
| strtoul base 16, 8 digits | newlib | 290 | 314 | 356 | ok |
| fu_parse_q16, 4 decimals | flexlib | 247 | 275 | 316 | ok |
| strtod * 65536 + lrint, 4 decimals | newlib | 786 | 1332 | 1413 | ok |
| fu_cobs_encode, 64 B random | flexlib | 329 | 329 | 423 | ok |
| cobs encode byte loop, 64 B random | candidate | 996 | 996 | 1003 | ok |
| fu_cobs_encode, 64 B 1/4 zeros | flexlib | 705 | 858 | 914 | ok |
| cobs encode byte loop, 64 B 1/4 zeros | candidate | 1080 | 1115 | 1129 | ok |
| fu_cobs_encode, 256 B random | flexlib | 1223 | 1234 | 1270 | ok |
| cobs encode byte loop, 256 B random | candidate | 3882 | 3886 | 3896 | ok |
| fu_cobs_decode, 64 B random | flexlib | 310 | 342 | 399 | ok |
| cobs decode byte loop, 64 B random | candidate | 906 | 906 | 925 | ok |
| fu_cobs_decode, 64 B 1/4 zeros | flexlib | 877 | 981 | 1115 | ok |
| cobs decode byte loop, 64 B 1/4 zeros | candidate | 1123 | 1177 | 1223 | ok |
| fu_cobs_rx_byte, 64 B frame + 0 | flexlib | 2217 | 2217 | 2227 | ok |
| fu_debounce32, 32 inputs per tick (inline) | flexlib | 20 | 20 | 20 | ok |
| fu_debounce, 1 input per tick (inline) | flexlib | 18 | 18 | 24 | ok |
| 16 x fu_debounce, 16 inputs per tick | candidate | 361 | 379 | 404 | ok |
| fu_xorshift32 (inline) | flexlib | 8 | 8 | 8 | ok |
| fu_xoshiro128ss (inline) | flexlib | 22 | 22 | 22 | ok |
| fu_xoshiro128_below(100), unbiased | flexlib | 33 | 33 | 33 | ok |
| xoshiro128** % 100 (UDIV, biased) | candidate | 36 | 38 | 38 | ok |
| rand() | newlib | 43 | 44 | 44 | - |
| fu_xoshiro128_jump (2^64 steps) | flexlib | 3268 | 3268 | 3268 | ok |
| fm_add_q31 (inline) | flexmath | 3 | 3 | 3 | ok |
| add q31, 64-bit clamp | candidate | 11 | 11 | 15 | ok |
| add q31, sign xor + IT | candidate | 8 | 8 | 8 | ok |
| fm_mul_q15 (inline) | flexmath | 6 | 6 | 6 | ok |
| fm_mul_q31 (inline) | flexmath | 15 | 17 | 17 | ok |
| fm_mul_q16 (inline) | flexmath | 16 | 18 | 18 | ok |
| mul q16, 64-bit clamp | candidate | 17 | 21 | 23 | ok |
| fm_f32_to_q15 | flexmath | 33 | 33 | 33 | ok |
| (q15)(x * 32768.0f), saturated | soft-float | 47 | 47 | 47 | ok |
| fm_q15_to_f32 | flexmath | 19 | 19 | 19 | ok |
| (float)q * 2^-15 | soft-float | 60 | 60 | 60 | ok |
| fm_f32_to_q31 | flexmath | 26 | 26 | 33 | ok |
| (q31)(x * 2^31), saturated | soft-float | 301 | 308 | 312 | ok |
| fm_q31_to_f32 | flexmath | 24 | 24 | 26 | ok |
| (float)q * 2^-31 | soft-float | 59 | 59 | 59 | ok |
| fm_isqrt32 | flexmath | 9 | 33 | 34 | ok |
| isqrt32 digit by digit | candidate | 77 | 130 | 187 | ok |
| isqrt32 Newton from a CLZ seed | candidate | 11 | 51 | 82 | ok |
| (uint32_t)sqrtf((float)x) | soft-float | 138 | 643 | 643 | - |
| fm_isqrt64 | flexmath | 13 | 38 | 82 | ok |
| isqrt64 digit by digit | candidate | 252 | 407 | 583 | ok |
| fm_sqrt_q15 | flexmath | 41 | 42 | 42 | ok |
| fm_sqrt_q31 | flexmath | 10 | 101 | 102 | ok |
| fm_sqrt_q16 | flexmath | 13 | 100 | 104 | ok |
| sqrtf on Q16.16 via float | soft-float | 183 | 699 | 699 | - |
| fm_rsqrt_q16 | flexmath | 10 | 115 | 119 | ok |
| 1/sqrtf on Q16.16 via float | soft-float | 202 | 823 | 823 | - |
| fm_div_q15 | flexmath | 22 | 33 | 35 | ok |
| (a << 15) / b, truncating | candidate | 13 | 18 | 21 | ok |
| fm_div_q31 | flexmath | 29 | 85 | 108 | ok |
| div q31, libgcc 64-bit core | candidate | 29 | 154 | 168 | ok |
| div q31, seed + Newton reciprocal core | candidate | 24 | 90 | 100 | ok |
| fm_div_q16 | flexmath | 31 | 51 | 105 | ok |
| div q16 without the 32-bit path | candidate | 30 | 88 | 100 | ok |
| (int64_t)a * 65536 / b, truncating | libgcc | 124 | 133 | 173 | ok |
| div q16 pre-shift SDIV (inexact) | candidate | 21 | 27 | 31 | ok |
| a / b in float, to Q16.16 | soft-float | 246 | 246 | 251 | ok |
| fm_recip_q16 | flexmath | 22 | 27 | 30 | ok |
| fm_div_q16(1, x) | candidate | 88 | 93 | 103 | ok |
| 2^32 / (int64_t)x, truncating | libgcc | 124 | 129 | 137 | ok |
| 1 / x in float, to Q16.16 | soft-float | 210 | 210 | 215 | ok |
| sinf | libm | 704 | 1348 | 1461 | - |
| cosf | libm | 847 | 1220 | 1597 | - |
| sqrtf | libm | 596 | 596 | 596 | - |
| atan2f | libm | 1130 | 1839 | 2005 | - |
| float multiply | libgcc | 40 | 40 | 40 | - |
| float divide | libgcc | 149 | 149 | 149 | - |
| arm_sin_q15 | CMSIS-DSP | 29 | 29 | 29 | - |
| arm_sin_f32 | CMSIS-DSP | 402 | 573 | 584 | - |
| arm_sqrt_q15 | CMSIS-DSP | 65 | 65 | 68 | - |
| arm_sqrt_q31 | CMSIS-DSP | 25 | 122 | 126 | - |
| arm_divide_q15 | CMSIS-DSP | 86 | 92 | 102 | - |
| arm_divide_q31 | CMSIS-DSP | 190 | 198 | 255 | - |

slow samples (above 2 x median + 4), by input index:

| case | samples | max | at input |
|---|--:|--:|---|
| fm_isqrt64 | 65 | 82 | 9, 12, 15, 17, ... |

golden: 161 ok, 0 FAIL, 0 missing

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
