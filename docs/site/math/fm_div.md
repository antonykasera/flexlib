<p class="fl-header-name">fm_div.h</p>

# Division

A fixed-point division needs more than the hardware divide: the dividend
has to be shifted up by the fraction bits first, which makes it a 64-bit
value. Through libgcc that is a 64-bit division of {cy}`(int64_t)a * 65536 / b, truncating`
cycles, and it truncates. `fm_div.h` divides in Q15, Q31 and Q16.16, rounds
to nearest, and handles division by zero.

## Three divisions

```{literalinclude} ../examples/fm_div.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_div.out
:language: text
```

1200 / 4800 is 0.25, which is 8192 in Q15. The inputs can be any two 16-bit
values; a ratio of 1 or more saturates. 10 / 3 is 3.33333..., and 3.33333... × 65536 is
218453.33, which rounds to 218453. Dividing -100 by 0 saturates toward the
sign of the dividend and gives -32768.

## Rounding and saturation

Every quotient rounds to nearest, within 0.5 LSB. A quotient outside the
format saturates to the nearest end. Division by 0 saturates the same way,
toward the dividend's sign, and 0 / 0 is 0. {c:func}`fm_recip_q16`
saturates for 0 and for ±2^-16, whose reciprocals do not fit, and 0 gives
`INT32_MAX`.

## How it was checked

- {c:func}`fm_div_q15` on all 2^32 pairs, and {c:func}`fm_recip_q16` on all
  2^32 inputs.
- {c:func}`fm_div_q31` and {c:func}`fm_div_q16` on 2^32 hashed pairs.

## Numbers

| function | cycles | CMSIS-DSP | libgcc | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|--:|
| `fm_div_q15` | {cy}`fm_div_q15` | {cy}`arm_divide_q15` |  | {acc}`divide q15; flexmath` | {acc}`divide q15; CMSIS-DSP` |
| `fm_div_q31` | {cy}`fm_div_q31` | {cy}`arm_divide_q31` |  | {acc}`divide q31; flexmath` | {acc}`divide q31; CMSIS-DSP` |
| `fm_div_q16` | {cy}`fm_div_q16` |  | {cy}`(int64_t)a * 65536 / b, truncating` | 0.5 |  |
| `fm_recip_q16` | {cy}`fm_recip_q16` |  | {cy}`2^32 / (int64_t)x, truncating` | 0.5 |  |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
CMSIS-DSP: `arm_divide_q15` and `arm_divide_q31`. libgcc: the truncating
64-bit division `(int64_t)a * 65536 / b`, and `2^32 / x`. CMSIS-DSP errors
for $|a| < |b|$.

## API

```{doxygenfunction} fm_div_q15
```

```{doxygenfunction} fm_div_q31
```

```{doxygenfunction} fm_div_q16
```

```{doxygenfunction} fm_recip_q16
```
