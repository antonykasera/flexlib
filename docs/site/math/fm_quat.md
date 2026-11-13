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
| `fm_quat_mul` | {cy}`fm_quat_mul` | {cy}`quat mul, float` | {cy}`arm_quaternion_product_single_f32` | {acc}`quaternion product f32; flexmath` ULP | {acc}`quaternion product f32; CMSIS-DSP` ULP |
| `fm_quat_normalize` | {cy}`fm_quat_normalize` | {cy}`quat normalize, 1 / sqrtf and 4 multiplies` | {cy}`arm_quaternion_normalize_f32, one` | {acc}`quaternion normalize f32; flexmath` ULP | {acc}`quaternion normalize f32; CMSIS-DSP` ULP |
| `fm_quat_rotate` | {cy}`fm_quat_rotate` | {cy}`quat rotate, float` | | 2^-22 \|v\| | |
| `fm_quat_integrate` | {cy}`fm_quat_integrate` | {cy}`quat integrate, float with sqrtf / sinf / cosf` |  | 2^-22 |  |

A first-order step plus normalisation takes {cy}`quat integrate, first order + normalise` cycles.

## Example

```{literalinclude} ../examples/fm_quat.c
:language: c
```
