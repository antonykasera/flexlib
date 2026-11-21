<p class="fl-header-name">fd_lms.h</p>

# LMS

Adaptive FIR filters in q15 and q31: plain LMS ($w += mu e x$), normalised
($w += mu e x / x'x$) and sign-error ($w += mu sign(e) x$).

Weights
: Q30 in 32 bits for both formats, so updates far below one output LSB
  still move them. Tap k of the q15 filter is the weight rounded to q15.

Output
: The exact sum of the taps times the samples, rounded once and saturated;
  $e = d - y$, saturated.

Update
: The step is mu e (normalised: mu e over x'x of the window, at most
  1 - 2^-15; sign-error: mu sign(e)), rounded once; the q15 plain step
  carries what the last rounding left off. Each weight takes step x exactly
  and saturates. The update for one sample runs in the same pass over the
  taps as the next sample's output; {c:func}`fd_lms_coefs_q15` applies the
  update still owed.

State
: The history plus a chunk of any size (`FD_LMS_STATE_LEN`); the chunk
  changes the speed, not the results.

Reset and step size
: {c:func}`fd_lms_reset_q15` clears the history and keeps the weights, with
  the update owed by the last sample applied. {c:func}`fd_lms_set_mu_q15`
  changes mu from the next sample's update on.

Asserts
: ntaps 0, a state buffer not longer than ntaps, $mu < 0$.

Checked
: - Outputs, errors and taps bit for bit against a 128-bit model: 6 lengths
    from 1 to 33 taps, all three modes, mu from 0 to the largest, random,
    full-scale and quiet input, blocks cut at random, taps read back at
    random points, resets and step-size changes at random points.
  - The normalised step on exact ties.
  - q15: an 8-tap filter identifies a 5-tap system in each mode, every tap
    within 200 LSB after 8000 samples.

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

## Example

```{literalinclude} ../examples/fd_lms.c
:language: c
```
