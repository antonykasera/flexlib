<p class="fl-header-name">fm_exp.h</p>

# Logarithms and exponentials

Logarithms, exponentials and powers in Q16.16.

Logarithms
: Within 0.501 LSB. $x <= 0$ gives `INT32_MIN`. {c:func}`fm_db_q16` is
  $20 log10(x)$, for amplitude ratios.

Exponentials
: Within 0.54 LSB. A result too large saturates; one too small rounds to 0.

Power
: {c:func}`fm_pow_q16` is within 0.65 LSB plus $|y| 2^-32$ of the result.
  $y = 0$ gives 1, $0^0$ included; otherwise $x < 0$ gives 0, and $0^y$ gives
  0, or `INT32_MAX` for $y < 0$.

Checked
: - The logarithms on all 2^31 positive inputs, {c:func}`fm_exp2_q16` and
    {c:func}`fm_exp_q16` on all 2^32, against `long double`.
  - {c:func}`fm_pow_q16` on 2^32 hashed pairs against `pow`.

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

## Example

```{literalinclude} ../examples/fm_exp.c
:language: c
```
