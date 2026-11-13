<p class="fl-header-name">fm_fixed.h</p>

# Fixed point

The Q formats, saturating add, subtract and multiply, a Q62 accumulator for
sums of Q31 products, and conversion to and from float. Everything but the
float conversion is inline.

Formats
: `fm_q15` is `int16_t` in $[-1, 1)$, `fm_q31` is `int32_t` in $[-1, 1)$,
  `fm_q16` is `int32_t` in $[-32768, 32768)$ with 16 fraction bits.

Rounding
: Products round to nearest, ties toward +inf. Float to Q rounds to nearest,
  ties to even; Q to float gives the nearest float, exact for Q15.

Saturation
: Every result saturates instead of wrapping; -1 * -1 gives the largest Q15
  or Q31 value. NaN converts to 0.

Accumulator
: {c:func}`fm_mac_q31` adds exact products in Q62 and does not saturate: the
  running sum must stay in $[-2, 2)$. {c:func}`fm_q62_to_q31` rounds and
  saturates once.

Checked
: - Q15 add, subtract and multiply on all 2^32 pairs.
  - Float to Q15, Q31 and Q16.16 on all 2^32 float bit patterns; Q31 and
    Q16.16 to float on all 2^32 values; Q15 to float on all 65536.
  - Q31 and Q16.16 arithmetic and the accumulator on 10^6 random pairs and
    the saturation corners, against 128-bit integers.

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

## Example

```{literalinclude} ../examples/fm_fixed.c
:language: c
```
