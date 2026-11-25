# flexmath

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexmath-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

Fixed-point and float math for Cortex-M parts without an FPU. The fixed-point
functions operate on Q15, Q31 and Q16.16. The float functions take and return
`float` and are implemented with integer instructions. Vectors, matrices,
quaternions and PID controllers are provided in both forms. Written in C11 with
no allocation.

## Example

```c
#include <math.h>
#include <stdio.h>

#include "fm_float.h"

int main(void) {
  float r = fm_sqrtf(3.0f * 3.0f + 4.0f * 4.0f);
  float a = fm_atan2f(4.0f, 3.0f);
  float s = fm_sinf(1e6f);
  printf("%g %.7f %.7f %.7f\n", (double)r, (double)a, (double)s, sin(1e6));
  return 0;
}
```

Output:

```text
5 0.9272952 -0.3499935 -0.3499935
```

## Fixed point

| Function | Cycles | Baseline | Max error | Baseline error |
| --- | --: | --- | --: | --: |
| `fm_mul_q15` / `fm_mul_q31` / `fm_mul_q16` | 6 / 17 / 18 | | 0.5 LSB | CMSIS 1 / 2 LSB |
| `fm_f32_to_q31` / `fm_q31_to_f32` | 26 / 24 | soft-float 309 / 59 | exact (nearest) | |
| `fm_isqrt32` | 33 | `sqrtf` via float 643 | exact floor | |
| `fm_sqrt_q15` / `fm_sqrt_q31` | 42 / 101 | CMSIS 65 / 122 | 0.5 LSB | 7.15 / 9 LSB |
| `fm_rsqrt_q16` | 115 | `1/sqrtf` via float 823 | 0.5 LSB | |
| `fm_div_q15` / `fm_div_q31` | 33 / 85 | CMSIS 92 / 198 | 0.5 LSB | 1 LSB |
| `fm_div_q16` / `fm_recip_q16` | 51 / 27 | libgcc 64-bit divide 133 / 129 | 0.5 LSB | truncates |
| `fm_sin_q15` / `fm_sin_q31` | 29 / 67 | CMSIS 29 / 46 | 0.65 / 1.11 LSB | 4.6 / 40400 LSB |
| `fm_sincos_q31` | 105 | two calls 143 | as sin, cos | |
| `fm_atan2_q15` / `fm_atan2_q31` | 63 / 181 | CMSIS 215 / 513 | 0.62 / 0.65 LSB | 3.7 / 4.8 LSB |
| `fm_asin_q15` / `fm_asin_q31` | 98 / 284 | | 0.75 / 0.81 LSB | |
| `fm_log2_q16` / `fm_ln_q16` | 72 / 87 | CMSIS `arm_vlog_q31` 548 | 0.501 LSB | 1.71 LSB |
| `fm_exp2_q16` / `fm_exp_q16` | 67 / 89 | `expf` via float 1566 | 0.54 LSB | |
| `fm_pow_q16` | 153 | `powf` via float 5183 | 0.65 LSB + \|y\| 2^-32 of the result | |

Angles are binary, with a full turn of 2^16 or 2^32, and a phase held in an
unsigned integer wraps without a range check. Results saturate. The logarithm
of x <= 0 returns `INT32_MIN`.

## Float

| Function | Cycles | newlib / libgcc | Max error, all 2^32 inputs | CMSIS-DSP |
| --- | --: | --: | --- | --- |
| `fm_sqrtf` | 63 | 595 | correctly rounded | |
| `fm_recipf` | 41 | `1.0f / x` 150 | correctly rounded | |
| `fm_rsqrtf` | 65 | `1.0f / sqrtf` 746 | correctly rounded | |
| `fm_sinf` / `fm_cosf` | 139 / 140 | 1348 / 1220 | 0.515 ULP | 573 cycles, 1.3e7 ULP |
| `fm_sincosf` | 204 | | 0.515 ULP | |
| `fm_atan2f` | 193 | 1839 | 0.545 ULP (2^32 hashed pairs) | 1461 cycles, 5.2 ULP |
| `fm_asinf` / `fm_acosf` | 267 / 276 | 1342 / 1463 | 0.545 ULP | |
| `fm_log2f` / `fm_logf` | 107 / 121 | 1701 / 1540 | 0.52 ULP | |
| `fm_exp2f` / `fm_expf` | 87 / 112 | 3546 / 1292 | 0.52 ULP | |

Infinities, NaN (quieted) and subnormals follow the C standard. `fm_sinf` and
`fm_cosf` reduce large arguments without loss of precision, in a constant 150
cycles.

## Vectors, matrices, quaternions, PID

| Function | Cycles | Plain C (libgcc) | CMSIS-DSP | Max error | Baseline error |
| --- | --: | --: | --: | --- | --- |
| `fm_mat4_mul_q31` | 992 | | 1309 | 0.5 LSB, saturated | 1 LSB, wraps |
| `fm_mat4_mul_f32` | 2986 | 6411 | 6445 | 0.5 ULP | 8e5 ULP |
| `fm_dot3_f32` / `fm_cross3_f32` | 207 / 425 | 264 / 461 | | 0.5 ULP | |
| `fm_quat_mul` | 747 | 1431 | 1434 | 0.5 ULP | 2e6 ULP |
| `fm_quat_normalize` | 534 | 937 | 914 | 0.6 ULP | 2.8 ULP |
| `fm_quat_rotate` | 843 | 1491 | | 2^-22 \|v\| | |
| `fm_quat_integrate` | 2083 | 5108 (libm) | | 2^-22 | |
| `fm_pid_q16` | 171 | | 39 \* | one rounding | |
| `fm_pid_f32` | 501 | 695 | 192 \* | one rounding per sum | |

\* CMSIS-DSP's PID is the velocity form, without output clamping or
anti-windup.

Sums of products are exact and rounded once. For float, the error
is at most 0.5 ULP plus 2^-32 times the sum of the products' magnitudes.
Matrices are row-major. `FM_Quat` is `{w, x, y, z}`. `fm_quat_integrate`
applies the exact rotation for the angular rate and renormalises. The PID
clamps its output, holds the integrator while clamped, and takes the
derivative on the measurement. Gains are per step, with ki = Ki dt and
kd = Kd / dt.

Cycles are medians on an STM32F103 at 72 MHz (`-O2`, warm cache). Full results
are in [docs/results/f103.md](../docs/results/f103.md), and the error
comparison with CMSIS-DSP in [docs/accuracy.md](../docs/accuracy.md).

## Configuration

| Macro | Effect |
| --- | --- |
| `FM_LUT_IN_RAM` | Places the tables (2.8 KB) in `.data`. A cold `fm_sin_q15` call takes 368 cycles instead of 480. |

## Usage

Select the library with `FLEXLIB_LIBS := math`, or link `flexlib::math` in
CMake. flexutils is added as a dependency. Build integration is described in
the [root README](../README.md#usage).

## Testing

See [docs/testing.md](../docs/testing.md).
