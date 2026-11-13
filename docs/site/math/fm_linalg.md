<p class="fl-header-name">fm_linalg.h</p>

# Vectors and matrices

Dot and cross products, and 3x3 and 4x4 matrices times a matrix or a
vector, in q31 and float.

Layout
: Matrices are row-major arrays. `r` must not overlap an input.

Rounding
: Every output is one exact sum of products, rounded once. q31: to nearest
  (ties up) and saturated, within 0.5 LSB, so a sum past 1 clamps instead
  of wrapping. float: within $0.5 ULP + 2^-32 sum |a_i b_i|$, so terms that
  cancel leave the exact remainder.

Special values
: float inf and NaN propagate as IEEE: inf x 0 and inf - inf give NaN.

Checked
: - q31 bit for bit against 128-bit sums on 300000 random cases each, with
    full scale, zero and +-1 often, so sums reach 4 and ties occur.
  - float against exact sums on 300000 random cases each, exponents within
    +-24 with zeros, subnormals and pairs that nearly cancel; overflow,
    underflow and the special values.

## API

### q31

```{doxygenfunction} fm_dot2_q31
```

```{doxygenfunction} fm_dot3_q31
```

```{doxygenfunction} fm_dot4_q31
```

```{doxygenfunction} fm_cross2_q31
```

```{doxygenfunction} fm_cross3_q31
```

```{doxygenfunction} fm_mat3_mul_q31
```

```{doxygenfunction} fm_mat3_vec_q31
```

```{doxygenfunction} fm_mat4_mul_q31
```

```{doxygenfunction} fm_mat4_vec_q31
```

### float

```{doxygenfunction} fm_dot2_f32
```

```{doxygenfunction} fm_dot3_f32
```

```{doxygenfunction} fm_dot4_f32
```

```{doxygenfunction} fm_cross2_f32
```

```{doxygenfunction} fm_cross3_f32
```

```{doxygenfunction} fm_mat3_mul_f32
```

```{doxygenfunction} fm_mat3_vec_f32
```

```{doxygenfunction} fm_mat4_mul_f32
```

```{doxygenfunction} fm_mat4_vec_f32
```

## Numbers

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache. The plain C column is
the same loop in float through libgcc. Errors over 200000 random q31
matrices in $[-1/4, 1/4)$ and 100000 random float matrices.

| function | cycles | plain C | CMSIS-DSP | max error | CMSIS-DSP |
|---|--:|--:|--:|--:|--:|
| `fm_dot3_q31` | 66 | | | 0.5 LSB | |
| `fm_cross3_q31` | 131 | | | 0.5 LSB | |
| `fm_mat3_vec_q31` | 189 | | | 0.5 LSB | |
| `fm_mat3_mul_q31` | 458 | | | 0.5 LSB | |
| `fm_mat4_vec_q31` | 283 | | | 0.5 LSB | |
| `fm_mat4_mul_q31` | 992 | | 1309 | 0.5 LSB | 1 LSB |
| `fm_dot3_f32` | 207 | 264 | | 0.5 ULP | |
| `fm_cross3_f32` | 425 | 461 | | 0.5 ULP | |
| `fm_mat3_vec_f32` | 591 | | | 0.5 ULP | |
| `fm_mat3_mul_f32` | 1428 | 2695 | | 0.5 ULP | |
| `fm_mat4_vec_f32` | 982 | | | 0.5 ULP | |
| `fm_mat4_mul_f32` | 2986 | 6411 | 6445 | 0.5 ULP | 8.05 10^5 ULP |

CMSIS-DSP: `arm_mat_mult_q31`, which wraps past full scale, and
`arm_mat_mult_f32`. With row 0 and column 0 nearly cancelling,
`arm_mat_mult_f32` reaches 6.9 10^10 ULP; `fm_mat4_mul_f32` stays at 0.5.

## Example

```{literalinclude} ../examples/fm_linalg.c
:language: c
```
