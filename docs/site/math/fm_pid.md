<p class="fl-header-name">fm_pid.h</p>

# PID

PID controllers in Q16.16 and float, with output limits and anti-windup.

Gains
: Per step: ki = Ki dt, kd = Kd / dt.

Derivative
: On the measurement, so a setpoint step does not kick the output.

Limits
: The output is clamped to [out_min, out_max]. While it is clamped, the
  integrator stops at the value that puts the output on the limit, so no
  wind-up builds up to be unwound later.

Rounding
: {c:func}`fm_pid_q16` sums exactly in 64 bits and rounds the output once
  (ties up). {c:func}`fm_pid_f32` keeps the integrator and the output each
  within 0.5 ULP + 2^-32 sum of their |terms|. A NaN or inf input gives NaN
  and leaves the state unchanged.

Asserts
: out_min > out_max.

Checked
: - {c:func}`fm_pid_q16` bit for bit against a 128-bit model on 40000
    random controllers of 32 steps each, gains and inputs including
    `INT32_MIN` and `INT32_MAX`.
  - {c:func}`fm_pid_f32` against exact sums on 20000 random controllers
    of 32 steps.
  - Both on an RC plant: settled at the setpoint, peak within 0.5% of it.

## API

```{doxygenstruct} FM_PidQ16
```

```{doxygenstruct} FM_PidF32
```

```{doxygenfunction} fm_pid_init_q16
```

```{doxygenfunction} fm_pid_reset_q16
```

```{doxygenfunction} fm_pid_q16
```

```{doxygenfunction} fm_pid_init_f32
```

```{doxygenfunction} fm_pid_reset_f32
```

```{doxygenfunction} fm_pid_f32
```

## Numbers

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles | plain C | CMSIS-DSP |
|---|--:|--:|--:|
| `fm_pid_q16` | 171 | | 39 |
| `fm_pid_f32` | 501 | 695 | 192 |

CMSIS-DSP's `arm_pid_q31` and `arm_pid_f32` are the velocity form with no
output limits, no anti-windup and the derivative on the error: a smaller
controller. Plain C is the same controller in float through libgcc.

## Example

```{literalinclude} ../examples/fm_pid.c
:language: c
```
