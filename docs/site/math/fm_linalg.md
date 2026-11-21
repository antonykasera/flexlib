<p class="fl-header-name">fm_linalg.h</p>

# Vectors and matrices

Sensor fusion and coordinate transforms are mostly small vectors and
matrices: a rotation applied to an accelerometer reading, a 4x4 transform, a
dot product. `fm_linalg.h` has dot and cross products, and 3x3 and 4x4
matrices times a matrix or a vector, in q31 and float. Each output is one
exact sum of its products, rounded once.

## A rotation, and a sum that cancels

```{literalinclude} ../examples/fm_linalg.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_linalg.out
:language: text
```

The q31 matrix turns a vector a quarter turn about z. Q31 cannot hold 1, so
the matrix uses the largest value, just under it. The vector 0.5 along x
still comes out exactly 0.5 along y, because $(2^31 - 1) 2^30 / 2^31$ rounds
to $2^30$.

The float dot product adds $10^8$, 1 and $-10^8$. Done one step at a time in
float, $10^8 + 1$ rounds back to $10^8$ and the 1 is lost, so plain float
gives 0. {c:func}`fm_dot3_f32` sums the products exactly and rounds once,
and gives 1.

## Layout and rounding

Matrices are row-major arrays, and the result must not overlap an input.
The q31 outputs round to nearest, ties up, within 0.5 LSB, and a sum past 1
saturates instead of wrapping. The float outputs are within
$0.5 ULP + 2^-32 sum |a_i b_i|$, so terms that cancel leave the exact
remainder, as in the example. Infinities and NaNs propagate as IEEE says:
inf × 0 and inf - inf give NaN.

## How it was checked

- q31 bit for bit against 128-bit sums on 300000 random cases each, with
  full scale, zero and ±1 often, so sums reach 4 and ties occur.
- Float against exact sums on 300000 random cases each: exponents within
  ±24 with zeros, subnormals and pairs that nearly cancel, overflow,
  underflow and the special values.

## Numbers

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache. The plain C column is
the same loop in float through libgcc. Errors over 200000 random q31
matrices in $[-1/4, 1/4)$ and 100000 random float matrices.

| function | cycles | plain C | CMSIS-DSP | max error | CMSIS-DSP |
|---|--:|--:|--:|--:|--:|
| `fm_dot3_q31` | {cy}`fm_dot3_q31` |  |  | 0.5 LSB |  |
| `fm_cross3_q31` | {cy}`fm_cross3_q31` |  |  | 0.5 LSB |  |
| `fm_mat3_vec_q31` | {cy}`fm_mat3_vec_q31` |  |  | 0.5 LSB |  |
| `fm_mat3_mul_q31` | {cy}`fm_mat3_mul_q31` |  |  | 0.5 LSB |  |
| `fm_mat4_vec_q31` | {cy}`fm_mat4_vec_q31` |  |  | 0.5 LSB |  |
| `fm_mat4_mul_q31` | {cy}`fm_mat4_mul_q31` |  | {cy}`arm_mat_mult_q31, 4x4` | {acc}`4x4 multiply q31; flexmath` LSB | {acc}`4x4 multiply q31; CMSIS-DSP` LSB |
| `fm_dot3_f32` | {cy}`fm_dot3_f32` | {cy}`dot3, float loop` |  | 0.5 ULP |  |
| `fm_cross3_f32` | {cy}`fm_cross3_f32` | {cy}`cross3, float` |  | 0.5 ULP |  |
| `fm_mat3_vec_f32` | {cy}`fm_mat3_vec_f32` |  |  | 0.5 ULP |  |
| `fm_mat3_mul_f32` | {cy}`fm_mat3_mul_f32` | {cy}`mat3 mul, float loop` |  | 0.5 ULP |  |
| `fm_mat4_vec_f32` | {cy}`fm_mat4_vec_f32` |  |  | 0.5 ULP |  |
| `fm_mat4_mul_f32` | {cy}`fm_mat4_mul_f32` | {cy}`mat4 mul, float loop` | {cy}`arm_mat_mult_f32, 4x4` | {acc}`4x4 multiply f32; flexmath; 100000 random matrices` ULP | {acc}`4x4 multiply f32; CMSIS-DSP; 100000 random matrices` ULP |

CMSIS-DSP: `arm_mat_mult_q31`, which wraps past full scale, and
`arm_mat_mult_f32`. With row 0 and column 0 nearly cancelling,
`arm_mat_mult_f32` reaches
{acc}`4x4 multiply f32; CMSIS-DSP; 100000 with row 0 . column 0 nearly cancelling` ULP; `fm_mat4_mul_f32` stays at
{acc}`4x4 multiply f32; flexmath; 100000 with row 0 . column 0 nearly cancelling`.

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
