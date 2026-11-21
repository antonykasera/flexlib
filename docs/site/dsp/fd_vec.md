<p class="fl-header-name">fd_vec.h</p>

# Vectors

Element-wise arithmetic, conversions, statistics and complex vectors over
blocks of q15 and q31, and float element-wise operations and sums.

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

Float
: The float element-wise operations do one float operation per element
  (libgcc on a part without an FPU). Negate, abs, fill and copy work on the
  bits. {c:func}`fd_dot_f32`, {c:func}`fd_power_f32`, {c:func}`fd_sum_f32`
  and {c:func}`fd_mean_f32` sum exactly and round once: within 0.5 ulp +
  $2^-32 sum |p|$. {c:func}`fd_rms_f32` is within 1 ulp. A NaN or inf in a
  sum gives NaN.

Aliasing
: `y` may equal an input of the same type.

Asserts
: A shift outside $[-15, 15]$ (q31: $[-31, 31]$), $lo > hi$, $n = 0$ where the
  result needs a sample, $n < 2$ or $n >= 2^24$ for variance.

Checked
: - Q15 add, subtract, multiply, complex multiply, {c:func}`fd_cmag_q15` and
    {c:func}`fd_cmag2_q15` on all 2^32 input pairs; {c:func}`fd_q31_to_q15`
    on all 2^32 values.
  - Every other function bit for bit against 128-bit references, on seeded
    random blocks with full-scale values at 14 lengths from 0 to 255, with
    guard bytes after the output; the binary operations also in place.
  - {c:func}`fd_db_q15` on every positive input, {c:func}`fd_db_q31` on 10^6
    random inputs.
  - The float sums against `__float128` on 4000 random blocks of 1 to 100
    values spread over up to 60 binades, within the bound every time.
    Cancellation, subnormals, overflow to inf, NaN and inf, sums past 2^62.
  - Float negate, abs and clip on signed zeros, NaN and inf.

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

```{doxygenfunction} fd_add_f32
```

```{doxygenfunction} fd_sub_f32
```

```{doxygenfunction} fd_mul_f32
```

```{doxygenfunction} fd_scale_f32
```

```{doxygenfunction} fd_offset_f32
```

```{doxygenfunction} fd_negate_f32
```

```{doxygenfunction} fd_abs_f32
```

```{doxygenfunction} fd_clip_f32
```

```{doxygenfunction} fd_fill_f32
```

```{doxygenfunction} fd_copy_f32
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

```{doxygenfunction} fd_dot_f32
```

```{doxygenfunction} fd_power_f32
```

```{doxygenfunction} fd_sum_f32
```

```{doxygenfunction} fd_mean_f32
```

```{doxygenfunction} fd_rms_f32
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
| `fd_add_q15` | {cy}`fd_add_q15, 64` | {cy}`arm_add_q15, 64` | exact | exact |
| `fd_add_q31` | {cy}`fd_add_q31, 64` | {cy}`arm_add_q31, 64` | exact | exact |
| `fd_mul_q15` | {cy}`fd_mul_q15, 64` | {cy}`arm_mult_q15, 64` | 0.5 | 1 |
| `fd_mul_q31` | {cy}`fd_mul_q31, 64` | {cy}`arm_mult_q31, 64` | 0.5 | 2 |
| `fd_scale_q15` | {cy}`fd_scale_q15, 64` | {cy}`arm_scale_q15, 64` | {acc}`scale q15; flexdsp` | {acc}`scale q15; CMSIS-DSP` |
| `fd_scale_q31` | {cy}`fd_scale_q31, 64` | {cy}`arm_scale_q31, 64` | 0.5 | 1 |
| `fd_shift_q15`, right by 3 | {cy}`fd_shift_q15 -3, 64` | {cy}`arm_shift_q15 -3, 64` | 0.5 | truncates |
| `fd_offset_q15` | {cy}`fd_offset_q15, 64` | {cy}`arm_offset_q15, 64` | exact | exact |
| `fd_negate_q15` | {cy}`fd_negate_q15, 64` | {cy}`arm_negate_q15, 64` | exact | exact |
| `fd_abs_q15` | {cy}`fd_abs_q15, 64` | {cy}`arm_abs_q15, 64` | exact | exact |
| `fd_clip_q15` | {cy}`fd_clip_q15, 64` | {cy}`arm_clip_q15, 64` | exact | exact |
| `fd_fill_q15` | {cy}`fd_fill_q15, 64` | {cy}`arm_fill_q15, 64` |  |  |
| `fd_copy_q15` | {cy}`fd_copy_q15, 64` | {cy}`arm_copy_q15, 64` |  |  |

`memcpy` of the same 128 bytes takes {cy}`memcpy 128 B` cycles in newlib-nano.

### Conversions

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_q31_to_q15` | {cy}`fd_q31_to_q15, 64` | {cy}`arm_q31_to_q15, 64` | {acc}`q31 to q15; flexdsp` | {acc}`q31 to q15; CMSIS-DSP` |
| `fd_q15_to_f32` | {cy}`fd_q15_to_f32, 64` | {cy}`arm_q15_to_float, 64` | exact |  |
| `fd_f32_to_q15` | {cy}`fd_f32_to_q15, 64` | {cy}`arm_float_to_q15, 64` | 0.5 |  |
| `fd_db_q15` | {cy}`fd_db_q15, 64` |  | 0.54 |  |

### Statistics

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_sum_q15` | {cy}`fd_sum_q15, 64` |  | exact |  |
| `fd_mean_q15` | {cy}`fd_mean_q15, 64` | {cy}`arm_mean_q15, 64` | 0.5 |  |
| `fd_dot_q15` | {cy}`fd_dot_q15, 64` | {cy}`arm_dot_prod_q15, 64` | exact | exact |
| `fd_dot_q31` | {cy}`fd_dot_q31, 64` | {cy}`arm_dot_prod_q31, 64` | {acc}`dot q31, Q16.48 out; flexdsp` | {acc}`dot q31, Q16.48 out; CMSIS-DSP` |
| `fd_power_q15` | {cy}`fd_power_q15, 64` | {cy}`arm_power_q15, 64` | exact | exact |
| `fd_power_q31` | {cy}`fd_power_q31, 64` | {cy}`arm_power_q31, 64` | {acc}`power q31, Q16.48 out; flexdsp` | {acc}`power q31, Q16.48 out; CMSIS-DSP` |
| `fd_rms_q15` | {cy}`fd_rms_q15, 64` | {cy}`arm_rms_q15, 64` | {acc}`rms q15; flexdsp` | {acc}`rms q15; CMSIS-DSP` |
| `fd_rms_q31` | {cy}`fd_rms_q31, 64` | {cy}`arm_rms_q31, 64` | 0.5 |  |
| `fd_var_q15` | {cy}`fd_var_q15, 64` | {cy}`arm_var_q15, 64` | {acc}`var q15; flexdsp` | {acc}`var q15; CMSIS-DSP` |
| `fd_std_q15` | {cy}`fd_std_q15, 64` | {cy}`arm_std_q15, 64` | {acc}`std q15; flexdsp` | {acc}`std q15; CMSIS-DSP` |
| `fd_std_q31` | {cy}`fd_std_q31, 64` |  | 0.5 |  |
| `fd_max_q15` | {cy}`fd_max_q15, 64` | {cy}`arm_max_q15, 64` | exact | exact |
| `fd_absmax_q15` | {cy}`fd_absmax_q15, 64` | {cy}`arm_absmax_q15, 64` | exact | exact |

### Float

Soft float on the F103. Sum errors in ulp of the exact result, over 10^5
blocks of values from 2^-10 to 2^10.

| function | cycles | CMSIS-DSP | max error, ulp | CMSIS-DSP, ulp |
|---|--:|--:|--:|--:|
| `fd_add_f32` | {cy}`fd_add_f32, 64` | {cy}`arm_add_f32, 64` | 0.5 | 0.5 |
| `fd_mul_f32` | {cy}`fd_mul_f32, 64` | {cy}`arm_mult_f32, 64` | 0.5 | 0.5 |
| `fd_abs_f32` | {cy}`fd_abs_f32, 64` | {cy}`arm_abs_f32, 64` | exact | exact |
| `fd_dot_f32` | {cy}`fd_dot_f32, 64` | {cy}`arm_dot_prod_f32, 64` | {acc}`dot f32, 64; flexdsp` | {acc}`dot f32, 64; CMSIS-DSP` |
| `fd_power_f32` | {cy}`fd_power_f32, 64` | {cy}`arm_power_f32, 64` | {acc}`power f32, 64; flexdsp` | {acc}`power f32, 64; CMSIS-DSP` |
| `fd_sum_f32` | {cy}`fd_sum_f32, 64` |  | {acc}`sum f32, 64; flexdsp` |  |
| `fd_mean_f32` | {cy}`fd_mean_f32, 64` | {cy}`arm_mean_f32, 64` | {acc}`mean f32, 64; flexdsp` | {acc}`mean f32, 64; CMSIS-DSP` |
| `fd_rms_f32` | {cy}`fd_rms_f32, 64` | {cy}`arm_rms_f32, 64` | {acc}`rms f32, 64; flexdsp` | {acc}`rms f32, 64; CMSIS-DSP` |

### Complex vectors

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_cmul_q15` | {cy}`fd_cmul_q15, 64` | {cy}`arm_cmplx_mult_cmplx_q15, 64` | {acc}`complex multiply q15, q15 out; flexdsp` | {acc}`complex multiply q15, Q3.13 out; CMSIS-DSP` (Q3.13) |
| `fd_cmul_q31` | {cy}`fd_cmul_q31, 32` | {cy}`arm_cmplx_mult_cmplx_q31, 32` | 0.5 |  |
| `fd_cmag_q15` | {cy}`fd_cmag_q15, 64` | {cy}`arm_cmplx_mag_q15, 64` | {acc}`complex magnitude q15, UQ1.15 out; flexdsp` | {acc}`complex magnitude q15, Q2.14 out; CMSIS-DSP` (Q2.14) |
| `fd_cmag_q31` | {cy}`fd_cmag_q31, 64` | {cy}`arm_cmplx_mag_q31, 64` | 0.5 |  |
| `fd_cmag2_q15` | {cy}`fd_cmag2_q15, 64` | {cy}`arm_cmplx_mag_squared_q15, 64` | exact (UQ2.30) | Q3.13 |
| `fd_cdot_q15` | {cy}`fd_cdot_q15, 64` |  | exact |  |

CMSIS-DSP errors are in its own output format.

## Example

```{literalinclude} ../examples/fd_vec.c
:language: c
```
