<p class="fl-header-name">fm_sqrt.h</p>

# Square roots

Square roots come up whenever you measure a level or a length: the RMS of a
signal, the magnitude of a vector, a normalisation. `fm_sqrt.h` has integer
square roots of 32- and 64-bit values, roots in Q15, Q31 and Q16.16, and
1/sqrt in Q16.16. The integer roots are exact, and the others are within
half an LSB.

## The RMS of a block

```{literalinclude} ../examples/fm_sqrt.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_sqrt.out
:language: text
```

`rms_q15` adds up the squares in 64 bits, where they are exact, divides by
the count, and takes {c:func}`fm_isqrt64` of the mean. A square wave of
±0.5 has an RMS of 0.5, which is 16384 in Q15. {c:func}`fm_rsqrt_q16` of 4
is 0.5, 32768 in Q16.16. The square root of a negative value is 0.

## Exact and rounded roots

{c:func}`fm_isqrt32` and {c:func}`fm_isqrt64` return the exact floor of the
square root: the largest integer whose square does not pass the input. The
fixed-point roots round to nearest, within 0.5 LSB, which is as close as the
format can hold.

An input of 0 or less gives 0. {c:func}`fm_rsqrt_q16` has no finite answer
there and returns `INT32_MAX`.

## How it was checked

- {c:func}`fm_isqrt32` on all 2^32 inputs. {c:func}`fm_sqrt_q31`,
  {c:func}`fm_sqrt_q16` and {c:func}`fm_rsqrt_q16` on all 2^31 - 1 positive
  inputs, and {c:func}`fm_sqrt_q15` on all 65536.
- {c:func}`fm_isqrt64` on 10^6 random squares of every size with their
  neighbours, 10^6 random values, and the edges of the range.

## Numbers

| function | cycles | CMSIS-DSP | soft-float | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|--:|
| `fm_isqrt32` | {cy}`fm_isqrt32` |  | {cy}`(uint32_t)sqrtf((float)x)` | exact |  |
| `fm_isqrt64` | {cy}`fm_isqrt64` |  |  | exact |  |
| `fm_sqrt_q15` | {cy}`fm_sqrt_q15` | {cy}`arm_sqrt_q15` |  | {acc}`sqrt q15; flexmath` | {acc}`sqrt q15; CMSIS-DSP` |
| `fm_sqrt_q31` | {cy}`fm_sqrt_q31` | {cy}`arm_sqrt_q31` |  | {acc}`sqrt q31; flexmath` | {acc}`sqrt q31; CMSIS-DSP` |
| `fm_sqrt_q16` | {cy}`fm_sqrt_q16` |  | {cy}`sqrtf on Q16.16 via float` | 0.5 |  |
| `fm_rsqrt_q16` | {cy}`fm_rsqrt_q16` |  | {cy}`1/sqrtf on Q16.16 via float` | 0.5 |  |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
soft-float: conversion to `float`, newlib's `sqrtf` (and a divide for
1/sqrt), conversion back.

## API

```{doxygenfunction} fm_isqrt32
```

```{doxygenfunction} fm_isqrt64
```

```{doxygenfunction} fm_sqrt_q15
```

```{doxygenfunction} fm_sqrt_q31
```

```{doxygenfunction} fm_sqrt_q16
```

```{doxygenfunction} fm_rsqrt_q16
```
