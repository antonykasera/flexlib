<p class="fl-header-name">fm_atan.h</p>

# Inverse trigonometry

The inverse functions turn measurements into angles: the phase of an I/Q
sample, the heading from two magnetometer axes, the tilt from one
accelerometer axis. `fm_atan.h` has atan2, asin and acos in Q15 and Q31,
and returns binary angles like [](fm_trig.md) uses.

## Phase and tilt

```{literalinclude} ../examples/fm_atan.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_atan.out
:language: text
```

The I/Q sample with I = 0.5 and Q = -0.5 points to -1/8 of a turn, which is -8192 when a
full turn is 65536, or -45 degrees. An accelerometer axis that reads 0.5 g
is tilted by asin(0.5), 30 degrees, which is 1/12 of a turn: 5461. At the
origin there is no direction, and atan2 returns 0.

## Ranges and accuracy

atan2 covers $[-1/2, 1/2)$ of a turn, asin $[-1/4, 1/4]$ and acos $[0, 1/2]$.
acos returns an unsigned type, because a half turn does not fit the signed
one. atan2 is within 0.65 LSB. asin and acos are within 0.75 LSB in Q15 and
0.81 LSB in Q31.

## How it was checked

- {c:func}`fm_atan2_q15` on all 2^32 pairs, {c:func}`fm_asin_q31` and
  {c:func}`fm_acos_q31` on all 2^32 inputs, and the Q15 asin and acos on all
  65536.
- {c:func}`fm_atan2_q31` on 2^32 hashed pairs. All against `long double`.

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
