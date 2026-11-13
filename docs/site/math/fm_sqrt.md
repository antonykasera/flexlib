<p class="fl-header-name">fm_sqrt.h</p>

# Square roots

Integer square roots of 32- and 64-bit values, square roots in Q15, Q31 and
Q16.16, and 1/sqrt in Q16.16.

Rounding
: {c:func}`fm_isqrt32` and {c:func}`fm_isqrt64` return the exact floor. The
  fixed-point roots round to nearest, within 0.5 LSB.

Domain
: x <= 0 gives 0; {c:func}`fm_rsqrt_q16` gives `INT32_MAX`.

Checked
: - {c:func}`fm_isqrt32` on all 2^32 inputs; {c:func}`fm_sqrt_q31`,
    {c:func}`fm_sqrt_q16` and {c:func}`fm_rsqrt_q16` on all 2^31 - 1
    positive inputs; {c:func}`fm_sqrt_q15` on all 65536.
  - {c:func}`fm_isqrt64` on 10^6 random squares of every size with their
    neighbours, 10^6 random values, and the edges of the range.

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

## Numbers

| function | cycles | CMSIS-DSP | soft-float | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|--:|
| `fm_isqrt32` | 33 | | 643 | exact | |
| `fm_isqrt64` | 39 | | | exact | |
| `fm_sqrt_q15` | 42 | 65 | | 0.5 | 7.15 |
| `fm_sqrt_q31` | 101 | 122 | | 0.5 | 9 |
| `fm_sqrt_q16` | 100 | | 699 | 0.5 | |
| `fm_rsqrt_q16` | 115 | | 823 | 0.5 | |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
soft-float: conversion to `float`, newlib's `sqrtf` (and a divide for
1/sqrt), conversion back.

## Example

```{literalinclude} ../examples/fm_sqrt.c
:language: c
```
