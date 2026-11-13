<p class="fl-header-name">fm_div.h</p>

# Division

Division in Q15, Q31 and Q16.16, and 1/x in Q16.16.

Rounding
: Quotients round to nearest, within 0.5 LSB.

Saturation
: A quotient outside the format saturates. Division by 0 saturates toward the
  dividend's sign; 0 / 0 is 0. {c:func}`fm_recip_q16` saturates for 0 and
  ±2^-16, and 0 gives `INT32_MAX`.

Checked
: - {c:func}`fm_div_q15` on all 2^32 pairs; {c:func}`fm_recip_q16` on all
    2^32 inputs.
  - {c:func}`fm_div_q31` and {c:func}`fm_div_q16` on 2^32 hashed pairs.

## API

```{doxygenfunction} fm_div_q15
```

```{doxygenfunction} fm_div_q31
```

```{doxygenfunction} fm_div_q16
```

```{doxygenfunction} fm_recip_q16
```

## Numbers

| function | cycles | CMSIS-DSP | libgcc | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|--:|
| `fm_div_q15` | 33 | 92 | | 0.5 | 1 |
| `fm_div_q31` | 85 | 198 | | 0.5 | 1 |
| `fm_div_q16` | 51 | | 133 | 0.5 | |
| `fm_recip_q16` | 27 | | 129 | 0.5 | |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
CMSIS-DSP: `arm_divide_q15` and `arm_divide_q31`. libgcc: the truncating
64-bit division `(int64_t)a * 65536 / b`, and `2^32 / x`. CMSIS-DSP errors
for |a| < |b|.

## Example

```{literalinclude} ../examples/fm_div.c
:language: c
```
