<p class="fl-header-name">fm_fixed.h</p>

# Fixed point

Fixed point keeps a fraction in an integer with an agreed scale. In Q15 the
integer 16384 means 16384 / 2^15, which is 0.5. Adding two Q15 values is an
integer add, and multiplying them is an integer multiply and a shift, so on
a part without an FPU the arithmetic costs a few cycles instead of a call
into libgcc. `fm_fixed.h` has the three formats flexlib uses, saturating
add, subtract and multiply, an accumulator for long sums of products, and
conversion to and from float.

## A dot product, and a product that does not fit

```{literalinclude} ../examples/fm_fixed.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_fixed.out
:language: text
```

`dot_q31` adds up the products with {c:func}`fm_mac_q31`, which keeps each
one exact in a 64-bit Q62 accumulator. Only {c:func}`fm_q62_to_q31` at the
end rounds, once. The result is exactly 0.75.

The second line multiplies -1 by -1 in Q15. The true answer, +1, is one
step past the largest Q15 value. {c:func}`fm_mul_q15` saturates and returns
32767, just under 1. A plain integer multiply and shift would give -1
instead, a sign flip that is far worse than an error of one LSB.

## The formats

`fm_q15` is an `int16_t` holding values in $[-1, 1)$. `fm_q31` is an
`int32_t` in $[-1, 1)$, with 16 more bits of precision. `fm_q16` is an
`int32_t` with 16 fraction bits, which holds $[-32768, 32768)$: use it for
quantities that are not fractions of full scale, such as volts or degrees.

## Rounding and saturation

Products round to nearest, with ties going up. Every result saturates
instead of wrapping, as the example shows. Conversion from float rounds to
nearest with ties to even, and NaN converts to 0. Conversion to float gives
the nearest float, which is exact for Q15.

The Q62 accumulator does not saturate. That is what keeps it exact and fast,
but it means the running sum must stay in $[-2, 2)$. With inputs below 1
that holds whenever the |values| on one side add up to less than 2, as in a
filter whose |coefficients| sum to less than 2.

## How it was checked

- Q15 add, subtract and multiply on all 2^32 pairs.
- Float to Q15, Q31 and Q16.16 on all 2^32 float bit patterns. Q31 and
  Q16.16 to float on all 2^32 values, and Q15 to float on all 65536.
- Q31 and Q16.16 arithmetic and the accumulator on 10^6 random pairs and
  the saturation corners, against 128-bit integers.

## Numbers

| function | cycles | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|
| `fm_add_q31` | {cy}`fm_add_q31 (inline)` | exact |  |
| `fm_mul_q15` | {cy}`fm_mul_q15 (inline)` | {acc}`multiply q15; flexmath` | {acc}`multiply q15; CMSIS-DSP` |
| `fm_mul_q31` | {cy}`fm_mul_q31 (inline)` | {acc}`multiply q31; flexmath` | {acc}`multiply q31; CMSIS-DSP` |
| `fm_mul_q16` | {cy}`fm_mul_q16 (inline)` | 0.5 |  |

| function | cycles | soft-float | max error |
|---|--:|--:|--:|
| `fm_f32_to_q15` | {cy}`fm_f32_to_q15` | {cy}`(q15)(x * 32768.0f), saturated` | {acc}`float to q15; flexmath` LSB |
| `fm_f32_to_q31` | {cy}`fm_f32_to_q31` | {cy}`(q31)(x * 2^31), saturated` | 0.5 LSB |
| `fm_q15_to_f32` | {cy}`fm_q15_to_f32` | {cy}`(float)q * 2^-15` | exact |
| `fm_q31_to_f32` | {cy}`fm_q31_to_f32` | {cy}`(float)q * 2^-31` | {acc}`q31 to float; flexmath` ULP |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
CMSIS-DSP: `arm_mult_q15` and `arm_mult_q31`, which truncate. soft-float: the
same scaling and saturation in C with `float` arithmetic.

## API

### Types

```{doxygentypedef} fm_q15
```

```{doxygentypedef} fm_q31
```

```{doxygentypedef} fm_q16
```

```{doxygendefine} FM_Q16_ONE
```

### Saturation

```{doxygenfunction} fm_sat_q15
```

```{doxygenfunction} fm_sat_q31
```

### Add and subtract

```{doxygenfunction} fm_add_q15
```

```{doxygenfunction} fm_sub_q15
```

```{doxygenfunction} fm_add_q31
```

```{doxygenfunction} fm_sub_q31
```

```{doxygenfunction} fm_add_q16
```

```{doxygenfunction} fm_sub_q16
```

### Multiply

```{doxygenfunction} fm_mul_q15
```

```{doxygenfunction} fm_mul_q31
```

```{doxygenfunction} fm_mul_q16
```

### Accumulate

```{doxygenfunction} fm_mac_q31
```

```{doxygenfunction} fm_q62_to_q31
```

### Float conversion

```{doxygenfunction} fm_f32_to_q15
```

```{doxygenfunction} fm_f32_to_q31
```

```{doxygenfunction} fm_f32_to_q16
```

```{doxygenfunction} fm_q15_to_f32
```

```{doxygenfunction} fm_q31_to_f32
```

```{doxygenfunction} fm_q16_to_f32
```
