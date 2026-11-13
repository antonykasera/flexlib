<p class="fl-header-name">fm_trig.h</p>

# Sine and cosine

Sine and cosine of binary angles, in Q15 and Q31.

Angles
: A full turn is 2^16 for the Q15 functions and 2^32 for Q31. Unsigned
  overflow wraps the way the angle does, so a phase accumulator needs no
  range reduction.

Error
: Q15 within 0.66 LSB, Q31 within 1.11 LSB. A result of 1 saturates to the
  largest value.

Pairs
: {c:func}`fm_sincos_q31` gives the same bits as {c:func}`fm_sin_q31` and
  {c:func}`fm_cos_q31`, in one call.

Checked
: - Q15 sine and cosine on all 65536 angles, Q31 sine on all 2^32, against
    `long double`.
  - The Q31 cosine bit-identical to the sine a quarter turn on; the pair
    functions bit-identical to the single calls.

## API

```{doxygenfunction} fm_sin_q15
```

```{doxygenfunction} fm_cos_q15
```

```{doxygenfunction} fm_sincos_q15
```

```{doxygenfunction} fm_sin_q31
```

```{doxygenfunction} fm_cos_q31
```

```{doxygenfunction} fm_sincos_q31
```

## Numbers

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fm_sin_q15` | 29 | 29 | 0.65 | 4.58 |
| `fm_cos_q15` | 30 | | 0.65 | |
| `fm_sincos_q15` | 63 | | 0.65 | |
| `fm_sin_q31` | 67 | 46 | 1.105 | 40400 |
| `fm_cos_q31` | 71 | | 1.105 | |
| `fm_sincos_q31` | 105 | | 1.105 | |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
CMSIS-DSP: `arm_sin_q15` and `arm_sin_q31`. With the flash cache flushed
before each call, {c:func}`fm_sin_q15` takes 480 cycles and
{c:func}`fm_sin_q31` 851; `FM_LUT_IN_RAM` moves the tables to RAM.

## Example

```{literalinclude} ../examples/fm_trig.c
:language: c
```
