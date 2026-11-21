<p class="fl-header-name">fm_quat.h</p>

# Quaternions

A gyro measures how fast a board turns. To know which way it points, you add
up those rates over time, and a quaternion is the usual way to hold the
result: four numbers with no gimbal lock. `fm_quat.h` has the float
operations an attitude filter needs: the product, normalisation, rotating a
vector, and integrating body rates.

## Integrating a gyro

The gyro here reads 90 degrees per second about z, for one second, sampled
at 1 kHz:

```{literalinclude} ../examples/fm_quat.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_quat.out
:language: text
```

After 1000 steps the body has turned a quarter turn, and its x axis points
along y. The -0.000002 is what 1000 steps of float rounding leave behind.

## Integration

{c:func}`fm_quat_integrate` rotates q by the body rates w (rad/s) over `dt`
with the exact rotation, not a first-order step, then renormalises. The
result is within 2^-22 for a unit q, and $|q|$ does not drift however many
steps you take: after 10^5 steps of 1 ms it is still within 2^-22 of 1.

## Product, normalisation and rotation

{c:func}`fm_quat_mul` is the Hamilton product. Each component is one exact
sum rounded once, within 0.5 ULP + 2^-32 of the sum of the |products|. `r`
may be one of the inputs, and an inf or NaN input gives NaN.
{c:func}`fm_quat_normalize` gives $q / |q|$ within 0.6 ULP for any finite q,
including those whose $|q|^2$ would overflow or underflow in float. Zero
gives the identity. {c:func}`fm_quat_rotate` computes $q v q*$ for a unit q,
within $2^-22 |v|$.

## How it was checked

- The product, normalisation, rotation and integration on 200000 random
  cases each against `long double`, with rates up to 35 rad/s and `dt` from
  0.1 to 20 ms.
- Aliasing, the special values, and $|q|^2$ overflow and underflow.
- 10^5 steps of 1 ms: $|q|$ within 2^-22 of 1.

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
