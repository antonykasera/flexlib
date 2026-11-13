<p class="fl-header-name">fd_vec.h</p>

# Vectors

Element-wise arithmetic, conversions, statistics and complex vectors over
blocks of q15 and q31.

Rounding
: To nearest, ties up, and saturated: within 0.5 LSB unless the function
  states otherwise.

Exact results
: Sums, q15 dot products and powers, {c:func}`fd_cdot_q15` and
  {c:func}`fd_cmag2_q15` are exact. RMS, variance and standard deviation are
  correctly rounded from exact sums; variance and standard deviation divide
  by n - 1.

Formats
: The q31 dot product and power return Q16.48, the CMSIS-DSP format,
  saturated past 2^15 (power: 2^16) instead of wrapping.
  {c:func}`fd_cmag_q15` and {c:func}`fd_cmag_q31` return UQ1.15 and UQ1.31,
  so magnitudes up to sqrt 2 do not saturate.

Aliasing
: `y` may equal an input of the same type.

Asserts
: A shift outside -15 to 15 (q31: -31 to 31), `lo > hi`, n = 0 where the
  result needs a sample, n < 2 or n >= 2^24 for variance.

Checked
: - Q15 add, subtract, multiply, complex multiply, {c:func}`fd_cmag_q15` and
    {c:func}`fd_cmag2_q15` on all 2^32 input pairs; {c:func}`fd_q31_to_q15`
    on all 2^32 values.
  - Every other function bit for bit against 128-bit references, on seeded
    random blocks with full-scale values at 14 lengths from 0 to 255, with
    guard bytes after the output; the binary operations also in place.
  - {c:func}`fd_db_q15` on every positive input, {c:func}`fd_db_q31` on 10^6
    random inputs.

## API

### Element-wise

```{doxygenfunction} fd_add_q15
```

```{doxygenfunction} fd_add_q31
```

```{doxygenfunction} fd_sub_q15
```

```{doxygenfunction} fd_sub_q31
```

```{doxygenfunction} fd_mul_q15
```

```{doxygenfunction} fd_mul_q31
```

```{doxygenfunction} fd_scale_q15
```

```{doxygenfunction} fd_scale_q31
```

```{doxygenfunction} fd_shift_q15
```

```{doxygenfunction} fd_shift_q31
```

```{doxygenfunction} fd_offset_q15
```

```{doxygenfunction} fd_offset_q31
```

```{doxygenfunction} fd_negate_q15
```

```{doxygenfunction} fd_negate_q31
```

```{doxygenfunction} fd_abs_q15
```

```{doxygenfunction} fd_abs_q31
```

```{doxygenfunction} fd_clip_q15
```

```{doxygenfunction} fd_clip_q31
```

```{doxygenfunction} fd_fill_q15
```

```{doxygenfunction} fd_fill_q31
```

```{doxygenfunction} fd_copy_q15
```

```{doxygenfunction} fd_copy_q31
```

### Conversions

```{doxygenfunction} fd_q15_to_q31
```

```{doxygenfunction} fd_q31_to_q15
```

```{doxygenfunction} fd_q7_to_q15
```

```{doxygenfunction} fd_q15_to_q7
```

```{doxygenfunction} fd_q15_to_f32
```

```{doxygenfunction} fd_f32_to_q15
```

```{doxygenfunction} fd_q31_to_f32
```

```{doxygenfunction} fd_f32_to_q31
```

```{doxygenfunction} fd_db_q15
```

```{doxygenfunction} fd_db_q31
```

### Statistics

```{doxygenfunction} fd_sum_q15
```

```{doxygenfunction} fd_sum_q31
```

```{doxygenfunction} fd_mean_q15
```

```{doxygenfunction} fd_mean_q31
```

```{doxygenfunction} fd_dot_q15
```

```{doxygenfunction} fd_dot_q31
```

```{doxygenfunction} fd_power_q15
```

```{doxygenfunction} fd_power_q31
```

```{doxygenfunction} fd_rms_q15
```

```{doxygenfunction} fd_rms_q31
```

```{doxygenfunction} fd_var_q15
```

```{doxygenfunction} fd_var_q31
```

```{doxygenfunction} fd_std_q15
```

```{doxygenfunction} fd_std_q31
```

```{doxygenfunction} fd_headroom_q15
```

```{doxygenfunction} fd_headroom_q31
```

```{doxygenfunction} fd_min_q15
```

```{doxygenfunction} fd_min_q31
```

```{doxygenfunction} fd_max_q15
```

```{doxygenfunction} fd_max_q31
```

```{doxygenfunction} fd_absmax_q15
```

```{doxygenfunction} fd_absmax_q31
```

```{doxygenfunction} fd_minmax_q15
```

```{doxygenfunction} fd_minmax_q31
```

### Complex vectors

```{doxygenfunction} fd_cmul_q15
```

```{doxygenfunction} fd_cmul_q31
```

```{doxygenfunction} fd_cmul_real_q15
```

```{doxygenfunction} fd_cmul_real_q31
```

```{doxygenfunction} fd_conj_q15
```

```{doxygenfunction} fd_conj_q31
```

```{doxygenfunction} fd_cmag_q15
```

```{doxygenfunction} fd_cmag_q31
```

```{doxygenfunction} fd_cmag2_q15
```

```{doxygenfunction} fd_cmag2_q31
```

```{doxygenfunction} fd_cdot_q15
```

```{doxygenfunction} fd_cdot_q31
```

```{doxygenfunction} fd_interleave_q15
```

```{doxygenfunction} fd_interleave_q31
```

```{doxygenfunction} fd_deinterleave_q15
```

```{doxygenfunction} fd_deinterleave_q31
```

## Numbers

Blocks of 64 (complex multiply q31: 32), medians on an STM32F103 at 72 MHz,
`-O2`, warm cache. Errors in LSB of the output format.

### Element-wise

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_add_q15` | 516 | 539 | exact | exact |
| `fd_add_q31` | 595 | 1137 | exact | exact |
| `fd_mul_q15` | 611 | 667 | 0.5 | 1 |
| `fd_mul_q31` | 1331 | 843 | 0.5 | 2 |
| `fd_scale_q15` | 590 | 510 | 0.5 | 1 |
| `fd_scale_q31` | 1280 | 986 | 0.5 | 1 |
| `fd_shift_q15`, right by 3 | 521 | 380 | 0.5 | truncates |
| `fd_offset_q15` | 413 | 452 | exact | exact |
| `fd_negate_q15` | 410 | 562 | exact | exact |
| `fd_abs_q15` | 585 | 619 | exact | exact |
| `fd_clip_q15` | 576 | 692 | exact | exact |
| `fd_fill_q15` | 144 | 138 | | |
| `fd_copy_q15` | 214 | 231 | | |

`memcpy` of the same 128 bytes takes 918 cycles in newlib-nano.

### Conversions

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_q31_to_q15` | 431 | 365 | 0.5 | 1 |
| `fd_q15_to_f32` | 1401 | 4051 | exact | |
| `fd_f32_to_q15` | 2359 | 3283 | 0.5 | |
| `fd_db_q15` | 3963 | | 0.54 | |

### Statistics

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_sum_q15` | 318 | | exact | |
| `fd_mean_q15` | 352 | 266 | 0.5 | |
| `fd_dot_q15` | 654 | 670 | exact | exact |
| `fd_dot_q31` | 1284 | 914 | 0.5 | 41.8 |
| `fd_power_q15` | 546 | 517 | exact | exact |
| `fd_power_q31` | 1205 | 817 | 0.5 | 41.1 |
| `fd_rms_q15` | 865 | 708 | 0.5 | 179 |
| `fd_rms_q31` | 1592 | 961 | 0.5 | |
| `fd_var_q15` | 1362 | 838 | 0.5 | 1 |
| `fd_std_q15` | 1411 | 916 | 0.5 | 178 |
| `fd_std_q31` | 2291 | | 0.5 | |
| `fd_max_q15` | 559 | 530 | exact | exact |
| `fd_absmax_q15` | 850 | 1097 | exact | exact |

### Complex vectors

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_cmul_q15` | 1712 | 1300 | 0.5 | 2 (Q3.13) |
| `fd_cmul_q31` | 2150 | 1167 | 0.5 | |
| `fd_cmag_q15` | 3331 | 8612 | 0.5 | 1 (Q2.14) |
| `fd_cmag_q31` | 7384 | 8848 | 0.5 | |
| `fd_cmag2_q15` | 718 | 958 | exact (UQ2.30) | Q3.13 |
| `fd_cdot_q15` | 1806 | | exact | |

CMSIS-DSP errors are in its own output format.

## Example

```{literalinclude} ../examples/fd_vec.c
:language: c
```
