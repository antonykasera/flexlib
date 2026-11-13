<p class="fl-header-name">fm_quat.h</p>

# Quaternions

Float quaternions for attitude: product, normalisation, vector rotation
and integration of body rates.

Product
: Hamilton product, each component one exact sum rounded once: within
  0.5 ULP + 2^-32 sum of the |products|. `r` may alias an input; inf or NaN
  in gives NaN.

Normalise
: $q / |q|$ within 0.6 ULP for any finite q, including those whose $|q|^2$
  overflows or underflows in float; zero gives the identity.

Rotate
: $q v q*$ for a unit q, within $2^-22 |v|$.

Integrate
: Rotates q by the body rates w (rad/s) over dt with the exact rotation,
  not a first-order step, then renormalises: within 2^-22 for a unit q, and
  |q| does not drift.

Checked
: - Product, normalisation, rotation and integration on 200000 random
    cases each against `long double`; rates to 35 rad/s, dt from 0.1 to
    20 ms.
  - Aliasing, the special values, |q|^2 overflow and underflow.
  - 10^5 steps of 1 ms: $|q|$ within 2^-22 of 1.

## API

```{doxygenstruct} FM_Quat
:members:
```

```{doxygenfunction} fm_quat_mul
```

```{doxygenfunction} fm_quat_normalize
```

```{doxygenfunction} fm_quat_rotate
```

```{doxygenfunction} fm_quat_integrate
```

## Numbers

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache. The plain C column is
the textbook formula in float through libgcc (integrate: with `sqrtf`,
`sinf` and `cosf` from newlib). Errors over 10^6 random quaternions.

| function | cycles | plain C | CMSIS-DSP | max error | CMSIS-DSP |
|---|--:|--:|--:|--:|--:|
| `fm_quat_mul` | 747 | 1431 | 1434 | 0.5 ULP | 1.96 10^6 ULP |
| `fm_quat_normalize` | 534 | 937 | 914 | 0.543 ULP | 2.79 ULP |
| `fm_quat_rotate` | 843 | 1491 | | 2^-22 \|v\| | |
| `fm_quat_integrate` | 2083 | 5108 | | 2^-22 | |

A first-order step plus normalisation takes 1522 cycles.

## Example

```{literalinclude} ../examples/fm_quat.c
:language: c
```
