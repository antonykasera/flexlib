<p class="fl-header-name">fd_lms.h</p>

# LMS

An adaptive filter learns its coefficients from the signals as they run. It
cancels an echo, removes hum picked up by a reference sensor, or identifies
an unknown system. The LMS algorithm is the simple, robust way to do it.
`fd_lms.h` has LMS filters in q15 and q31, in three forms: plain LMS
($w += mu e x$), normalised ($w += mu e x / x'x$) and sign-error
($w += mu sign(e) x$).

## Identifying a system

Here an unknown 4-tap system is driven by noise, and a normalised LMS
filter is given its input x and its output d:

```{literalinclude} ../examples/fd_lms.c
:language: c
```

Which prints

```{literalinclude} ../examples/fd_lms.out
:language: text
```

After 4000 samples the filter's taps equal the system's exactly, 0 LSB
apart. {c:func}`fd_lms_coefs_q15` reads them out. The step size mu is 0.25,
8192 in q15. With the normalised form the step does not depend on the
input's level, which makes mu easier to choose.

## Weights and output

The weights are Q30 in 32 bits for both formats. Updates far smaller than
one output LSB still move them, which is what lets a q15 filter keep
learning after it has nearly converged. Tap k of the q15 filter is the
weight rounded to q15.

Each output is the exact sum of the taps times the samples, rounded once and
saturated, and the error $e = d - y$ is saturated too.

## The update

The step for each sample is mu e (normalised: mu e over the window's x'x, at
most 1 - 2^-15; sign-error: mu sign(e)), rounded once. The q15 plain step
also carries what the last rounding left off, so small steps are not lost.
Each weight then takes step × x exactly and saturates.

To save a pass over the taps, the update for one sample runs in the same
loop as the next sample's output. {c:func}`fd_lms_coefs_q15` applies the
update still owed before it reads the taps out.

## State, reset and step size

The state buffer holds the history plus a chunk of any size
(`FD_LMS_STATE_LEN`). The chunk changes the speed, not the results.

{c:func}`fd_lms_reset_q15` clears the history and keeps the weights, with
the update owed by the last sample applied: use it when the signal stops
and restarts. {c:func}`fd_lms_set_mu_q15` changes mu from the next sample's
update on, for example to learn fast at first and settle with a smaller
step. A zero `ntaps`, a state buffer no longer than `ntaps`, and a negative
mu stop at an assert.

## How it was checked

- Outputs, errors and taps bit for bit against a 128-bit model: 6 lengths
  from 1 to 33 taps, all three forms, mu from 0 to the largest, random,
  full-scale and quiet input, blocks cut at random, the taps read back, the
  filter reset and mu changed, at random points.
- The normalised step on exact ties.
- q15: an 8-tap filter identifies a 5-tap system in each form, every tap
  within 200 LSB after 8000 samples.

## Numbers

32 taps, blocks of 64, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles | CMSIS-DSP |
|---|--:|--:|
| `fd_lms_q15` | {cy}`fd_lms_q15, 32 taps x 64` | {cy}`arm_lms_q15, 32 taps x 64` |
| `fd_lms_q15`, normalised | {cy}`fd_lms_q15 normalised, 32 taps x 64` | {cy}`arm_lms_norm_q15, 32 taps x 64` |
| `fd_lms_q15`, sign-error | {cy}`fd_lms_q15 sign-error, 32 taps x 64` |  |
| `fd_lms_q31` | {cy}`fd_lms_q31, 32 taps x 64` | {cy}`arm_lms_q31, 32 taps x 64` |
| `fd_lms_q31`, normalised | {cy}`fd_lms_q31 normalised, 32 taps x 64` |  |
| `fd_lms_q31`, sign-error | {cy}`fd_lms_q31 sign-error, 32 taps x 64` |  |

Identifying a 16-tap system with noise 60 dB down at $mu = 2^-10$, weight
error in dB below the system's norm, with a `double` LMS on the same data:

| samples | q15 | CMSIS-DSP q15 | q31 | CMSIS-DSP q31 | double |
|--:|--:|--:|--:|--:|--:|
| 2048 | {acc}`2048 @ flexdsp q15` | {acc}`2048 @ CMSIS-DSP q15` | {acc}`2048 @ flexdsp q31` | {acc}`2048 @ CMSIS-DSP q31` | {acc}`2048 @ double` |
| 8192 | {acc}`8192 @ flexdsp q15` | {acc}`8192 @ CMSIS-DSP q15` | {acc}`8192 @ flexdsp q31` | {acc}`8192 @ CMSIS-DSP q31` | {acc}`8192 @ double` |
| 32768 | {acc}`32768 @ flexdsp q15` | {acc}`32768 @ CMSIS-DSP q15` | {acc}`32768 @ flexdsp q31` | {acc}`32768 @ CMSIS-DSP q31` | {acc}`32768 @ double` |
| 59968 | {acc}`59968 @ flexdsp q15` | {acc}`59968 @ CMSIS-DSP q15` | {acc}`59968 @ flexdsp q31` | {acc}`59968 @ CMSIS-DSP q31` | {acc}`59968 @ double` |

CMSIS-DSP's q15 LMS truncates its updates and drifts.

## API

```{doxygenenum} FD_LmsMode
```

```{doxygendefine} FD_LMS_STATE_LEN
```

```{doxygenstruct} FD_LmsQ15
```

```{doxygenfunction} fd_lms_init_q15
```

```{doxygenfunction} fd_lms_q15
```

```{doxygenfunction} fd_lms_coefs_q15
```

```{doxygenfunction} fd_lms_reset_q15
```

```{doxygenfunction} fd_lms_set_mu_q15
```

```{doxygenstruct} FD_LmsQ31
```

```{doxygenfunction} fd_lms_init_q31
```

```{doxygenfunction} fd_lms_q31
```

```{doxygenfunction} fd_lms_coefs_q31
```

```{doxygenfunction} fd_lms_reset_q31
```

```{doxygenfunction} fd_lms_set_mu_q31
```
