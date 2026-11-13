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
| `fm_atan2_q15` | {cy}`fm_atan2_q15` | {cy}`arm_atan2_q15` | {acc}`atan2 q15; flexmath` | {acc}`atan2 q15; CMSIS-DSP` |
| `fm_asin_q15` | {cy}`fm_asin_q15` |  | 0.747 |  |
| `fm_acos_q15` | {cy}`fm_acos_q15` |  | 0.747 |  |
| `fm_atan2_q31` | {cy}`fm_atan2_q31` | {cy}`arm_atan2_q31` | 0.646 | {acc}`atan2 q31; CMSIS-DSP` |
| `fm_asin_q31` | {cy}`fm_asin_q31` |  | 0.800 |  |
| `fm_acos_q31` | {cy}`fm_acos_q31` |  | 0.800 |  |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
CMSIS-DSP: `arm_atan2_q15` and `arm_atan2_q31`; its errors are measured in
the same angle units.

## Example

```{literalinclude} ../examples/fm_atan.c
:language: c
```
