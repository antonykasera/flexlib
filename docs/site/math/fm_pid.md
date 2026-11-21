<p class="fl-header-name">fm_pid.h</p>

# PID

A PID controller closes a loop: it reads a measurement, compares it with a
setpoint, and sets an output, such as a heater's PWM duty or a motor's
voltage, to bring the two together. `fm_pid.h` has PID controllers in
Q16.16 and float, with the details a real loop needs: output limits,
anti-windup and a derivative that does not kick when the setpoint changes.

## Regulating a voltage

Here a PI controller sets the PWM duty, 0 to 1, of a 3.3 V output through an
RC filter, and holds the filtered voltage at 1.65 V:

```{literalinclude} ../examples/fm_pid.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_pid.out
:language: text
```

The voltage settles at 1.6500 V, and its peak is 1.6500 V: there is no
overshoot. At the start the controller asks for more than full duty, and
the output is clamped at 1. A plain PI would keep growing its integral term
during that time and overshoot once the voltage arrives. Here the
integrator stops at the value that puts the output on the limit, so there
is nothing to unwind.

## Gains

The gains are per step: $ki = Ki dt$ and $kd = Kd / dt$, with dt the
interval between calls. The example's ki of 983 is 0.015 per step in
Q16.16.

## Derivative and limits

The derivative acts on the measurement, not on the error, so a step in the
setpoint does not make the output jump. The output is clamped to
$[out_min, out_max]$, and while it is clamped the integrator holds still,
as the example shows. $out_min > out_max$ stops at an assert.

## Rounding

{c:func}`fm_pid_q16` sums exactly in 64 bits and rounds the output once,
ties up. {c:func}`fm_pid_f32` keeps the integrator and the output each within
0.5 ULP + 2^-32 of the sum of their |terms|. A NaN or inf input gives NaN and
leaves the controller's state unchanged, so one bad sample does not spoil
the loop.

## How it was checked

- {c:func}`fm_pid_q16` bit for bit against a 128-bit model on 40000 random
  controllers of 32 steps each, with gains and inputs including `INT32_MIN`
  and `INT32_MAX`.
- {c:func}`fm_pid_f32` against exact sums on 20000 random controllers of 32
  steps.
- Both on an RC plant: settled at the setpoint, with a peak within 0.5% of
  it.

## Numbers

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles | plain C | CMSIS-DSP |
|---|--:|--:|--:|
| `fm_pid_q16` | {cy}`fm_pid_q16` |  | {cy}`arm_pid_q31 (no clamps, no anti-windup)` |
| `fm_pid_f32` | {cy}`fm_pid_f32` | {cy}`pid f32, the same in plain float` | {cy}`arm_pid_f32 (no clamps, no anti-windup)` |

CMSIS-DSP's `arm_pid_q31` and `arm_pid_f32` are the velocity form with no
output limits, no anti-windup and the derivative on the error: a smaller
controller. Plain C is the same controller in float through libgcc.

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
