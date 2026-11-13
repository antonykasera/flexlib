<p class="fl-header-name">fm_atan.h</p>

# Inverse trigonometry

atan2, asin and acos in Q15 and Q31, returning binary angles.

Angles
: As in [](fm_trig.md): a full turn is 2^16 for the Q15 functions and 2^32
  for Q31. atan2 covers $[-1/2, 1/2)$ turn, asin $[-1/4, 1/4]$, acos $[0, 1/2]$;
  acos returns an unsigned type to hold 1/2.

Error
: atan2 within 0.65 LSB; asin and acos within 0.75 LSB in Q15, 0.81 in Q31.

Origin
: $atan2(0, 0) = 0$.

Checked
: - {c:func}`fm_atan2_q15` on all 2^32 pairs; {c:func}`fm_asin_q31` and
    {c:func}`fm_acos_q31` on all 2^32 inputs; the Q15 asin and acos on all
    65536.
  - {c:func}`fm_atan2_q31` on 2^32 hashed pairs. All against `long double`.

## API

```{doxygenfunction} fm_atan2_q15
```

```{doxygenfunction} fm_asin_q15
```

```{doxygenfunction} fm_acos_q15
```

```{doxygenfunction} fm_atan2_q31
```

```{doxygenfunction} fm_asin_q31
```

```{doxygenfunction} fm_acos_q31
```

## Numbers

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fm_atan2_q15` | 63 | 215 | 0.615 | 3.68 |
| `fm_asin_q15` | 98 | | 0.747 | |
| `fm_acos_q15` | 99 | | 0.747 | |
| `fm_atan2_q31` | 181 | 513 | 0.646 | 4.81 |
| `fm_asin_q31` | 284 | | 0.800 | |
| `fm_acos_q31` | 283 | | 0.800 | |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
CMSIS-DSP: `arm_atan2_q15` and `arm_atan2_q31`; its errors are measured in
the same angle units.

## Example

```{literalinclude} ../examples/fm_atan.c
:language: c
```
