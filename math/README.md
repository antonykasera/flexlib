# flexmath

Math for Cortex-M parts without an FPU: fixed point (Q15, Q31, Q16.16), float
functions on integer internals, and vectors, matrices, quaternions and PID.
Plain C11, no allocation.

Cycles are medians on an STM32F103 at 72 MHz (`-O2`, warm cache), from
[docs/results/f103.md](../docs/results/f103.md). Errors are against a `double`
or exact reference; CMSIS-DSP errors are in
[docs/accuracy.md](../docs/accuracy.md).

## Fixed point

| function | cycles | baseline | max error | baseline's error |
|---|--:|---|--:|--:|
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
| `fm_pow_q16` | 153 | `powf` via float 5183 | 0.65 LSB + \|y\| 2^-32 | |

Angles in `fm_trig` and `fm_atan` are binary (a full turn is 2^16 or 2^32).
Results saturate; the logs of x <= 0 return `INT32_MIN`.

## Float without an FPU

Each takes and returns `float` and uses only integer operations. inf, NaN
(quieted) and subnormals are handled.

| function | cycles | newlib / libgcc | max error, on all 2^32 floats | CMSIS-DSP |
|---|--:|--:|---|---|
| `fm_sqrtf` | 63 | 595 | correctly rounded | |
| `fm_recipf` | 41 | `1.0f / x` 150 | correctly rounded | |
| `fm_rsqrtf` | 65 | `1.0f / sqrtf` 746 | correctly rounded | |
| `fm_sinf` / `fm_cosf` | 139 / 140 | 1348 / 1220 | 0.515 ULP, any finite x | 573 cycles, 1.3e7 ULP |
| `fm_sincosf` | 204 | | 0.515 ULP | |
| `fm_atan2f` | 193 | 1839 | 0.545 ULP (2^32 hashed pairs) | 1461 cycles, 5.2 ULP |
| `fm_asinf` / `fm_acosf` | 267 / 276 | 1342 / 1463 | 0.545 ULP | |
| `fm_log2f` / `fm_logf` | 107 / 121 | 1701 / 1540 | 0.52 ULP | |
| `fm_exp2f` / `fm_expf` | 87 / 112 | 3546 / 1292 | 0.52 ULP | |

`fm_sinf` and `fm_cosf` reduce any argument exactly, so large arguments cost
the same 150 cycles; newlib's `sinf` takes 15320 for |x| between pi/4 and 2^22.

## Vectors, matrices, quaternions, PID

Every output is one exact sum of products rounded once. For float the error
is at most 0.5 ULP + 2^-32 times the sum of the products' magnitudes.

| function | cycles | plain C (libgcc) | CMSIS-DSP | error | CMSIS / plain error |
|---|--:|--:|--:|---|---|
| `fm_mat4_mul_q31` | 992 | | 1309 | 0.5 LSB, saturated | 1 LSB, wraps |
| `fm_mat3_mul_q31` / `fm_dot3_q31` | 458 / 66 | | | 0.5 LSB | |
| `fm_mat4_mul_f32` | 2986 | 6411 | 6445 | 0.5 ULP | 8e5 ULP on random matrices |
| `fm_mat3_mul_f32` | 1428 | 2695 | | 0.5 ULP | |
| `fm_dot3_f32` / `fm_cross3_f32` | 207 / 425 | 264 / 461 | | 0.5 ULP | |
| `fm_quat_mul` | 747 | 1431 | 1434 | 0.5 ULP | 2e6 ULP |
| `fm_quat_normalize` | 534 | 937 | 914 | 0.6 ULP, any finite q | 2.8 ULP |
| `fm_quat_rotate` | 843 | 1491 | | 2^-22 \|v\| | |
| `fm_quat_integrate` | 2083 | 5108 (libm) | | 2^-22, exact rotation | |
| `fm_pid_q16` | 147 | | 39 * | exact, one rounding | |
| `fm_pid_f32` | 442 | 695 | 192 * | one rounding per sum | |

\* CMSIS's PID is the velocity form with no output clamping, no anti-windup and
derivative on the error: a smaller controller than flexmath's.

- `fm_linalg`: dot2/3/4, cross2/3, mat3 and mat4 times a matrix or a vector;
  row-major arrays.
- `fm_quat`: `FM_Quat {w, x, y, z}`; `fm_quat_integrate` applies the exact
  rotation and renormalises.
- `fm_pid`: integrator clamped to the output range, output clamped, derivative
  on the measurement. Gains are per step: ki = Ki dt, kd = Kd / dt.

## Use

In a [flex](https://github.com/antonykasera/STM32F1-HAL) project (v1.1.0 or
later), one line in the Makefile (flexutils comes with it):

```make
FLEXLIB_LIBS := math
```

Elsewhere, include the fragment (it only sets variables):

```make
include lib/flexlib/flexmath.mk     # pulls in flexutils too
CFLAGS += $(FLEXLIB_CFLAGS)
SRCS   += $(FLEXLIB_SRCS)
```

```c
#include "fm_pid.h"
#include "fm_quat.h"

static FM_Quat att = {1.0f, 0.0f, 0.0f, 0.0f};
static FM_PidF32 tilt;

void control_init(void) {
  fm_pid_init_f32(&tilt, 2.0f, 0.01f, 0.5f, -1.0f, 1.0f);
  fm_pid_reset_f32(&tilt, 0.0f);
}

/* every 1 ms, gyro in rad/s: hold the body's z axis upright in y */
float control_step(const float gyro[3]) {
  static const float z[3] = {0.0f, 0.0f, 1.0f};
  float up[3];
  fm_quat_integrate(&att, &att, gyro, 0.001f);
  fm_quat_rotate(up, &att, z);
  return fm_pid_f32(&tilt, 0.0f, up[1]);
}
```

No soft-float anywhere in that loop: about 3.4k cycles, 47 us at 72 MHz.

## Options

- `FM_LUT_IN_RAM`: puts the tables (2.8 KB) in `.data`; a cold `fm_sin_q15`
  drops from 480 to 368 cycles.
- `FX_ASSERT_FAIL(file, line)` / `FX_DISABLE_ASSERT`: argument checks trap by
  default, with no stdio.

## Checking it

`make` runs the host tests, the cross builds and the asm audit;
`make exhaustive` runs every 32-bit input; `make accuracy` writes the
CMSIS-DSP comparison; `make bench-run` measures on the board. See the
[top-level README](../README.md).
