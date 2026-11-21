<p class="fl-header-name">fm_exp.h</p>

# Logarithms and exponentials

Levels in dB, the decay of an RC circuit, a sensor's power law: these need
logarithms and exponentials, and converting to float for `logf` and `expf`
costs over a thousand cycles each on a part without an FPU. `fm_exp.h`
computes them in Q16.16, to within about half an LSB.

## A level, a decay and a power

```{literalinclude} ../examples/fm_exp.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_exp.out
:language: text
```

The level of 0.1 against full scale is printed as -20.0008 dB rather than
-20. That is not an error: 0.1 is not exact in Q16.16, and the value held,
6553 / 65536, is 0.099990, whose level really is -20.0008 dB. The voltage of
an RC circuit after one time constant is $e^-1$, 0.3679, and $2^0.5$ is
1.4142.

## Logarithms

{c:func}`fm_log2_q16`, {c:func}`fm_ln_q16` and {c:func}`fm_log10_q16` are
within 0.501 LSB. {c:func}`fm_db_q16` is $20 log10(x)$, the form for
amplitude ratios. There is no logarithm of 0 or of a negative value, and
they return `INT32_MIN`.

## Exponentials and powers

{c:func}`fm_exp2_q16` and {c:func}`fm_exp_q16` are within 0.54 LSB. A
result too large saturates, and one too small rounds to 0.

{c:func}`fm_pow_q16` computes $x^y$ within 0.65 LSB, plus $|y| 2^-32$ of
the result. The extra term only matters for large exponents. $y = 0$ gives
1, $0^0$ included. Otherwise a negative x gives 0, and $0^y$ gives 0, or
`INT32_MAX` for $y < 0$.

## How it was checked

- The logarithms on all 2^31 positive inputs, and {c:func}`fm_exp2_q16` and
  {c:func}`fm_exp_q16` on all 2^32, against `long double`.
- {c:func}`fm_pow_q16` on 2^32 hashed pairs against `pow`.

## Numbers

| function | cycles | CMSIS-DSP | libm | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|--:|
| `fm_log2_q16` | {cy}`fm_log2_q16` |  | {cy}`log2f via float` | 0.5000 |  |
| `fm_ln_q16` | {cy}`fm_ln_q16` | {cy}`arm_vlog_q31, one value (ln, q5.26 out)` | {cy}`logf via float` | 0.5001 | {acc}`ln, q5.26 out; CMSIS-DSP` |
| `fm_log10_q16` | {cy}`fm_log10_q16` |  |  | 0.5001 |  |
| `fm_db_q16` | {cy}`fm_db_q16` |  |  | 0.5005 |  |
| `fm_exp2_q16` | {cy}`fm_exp2_q16` |  | {cy}`exp2f via float` | 0.5306 |  |
| `fm_exp_q16` | {cy}`fm_exp_q16` |  | {cy}`expf via float` | 0.5366 |  |
| `fm_pow_q16` | {cy}`fm_pow_q16` |  | {cy}`powf via float` |  |  |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
CMSIS-DSP: `arm_vlog_q31` on one value, with a q5.26 result. libm: conversion
to `float`, newlib's `log2f`, `logf`, `exp2f`, `expf` or `powf`, conversion
back. {c:func}`fm_pow_q16` stays within 0.77 of its bound on 2^32 hashed
pairs.

## API

### Logarithms

```{doxygenfunction} fm_log2_q16
```

```{doxygenfunction} fm_ln_q16
```

```{doxygenfunction} fm_log10_q16
```

```{doxygenfunction} fm_db_q16
```

### Exponentials

```{doxygenfunction} fm_exp2_q16
```

```{doxygenfunction} fm_exp_q16
```

```{doxygenfunction} fm_pow_q16
```
