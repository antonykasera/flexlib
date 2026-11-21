<p class="fl-header-name">fd_fir.h</p>

# FIR filters

FIR filters, decimators and interpolators in q15 and q31, float FIR
filters, and windowed-sinc design in float.

Rounding
: Every output is the exact sum rounded once (0.5 LSB) and saturated, for
  any coefficients.

Order
: `h[0]` multiplies the newest sample.

State
: The caller's buffer holds the `ntaps - 1` history samples plus a chunk of
  any size (`FD_FIR_STATE_LEN`). A block runs in chunk-sized pieces and the
  history moves once per chunk; the chunk changes the speed, not the
  results.

Blocks
: Any `n`. Decimators return the outputs written; interpolators write `n l`
  outputs and must not overlap their input.

Float
: {c:func}`fd_fir_f32` runs the q31 kernel on block floating point, with one
  rounding per output:
  $|y - sum h x| <= 0.5 ulp(y) + 2^-31 sum |h| (X + sum |x|)$, X the largest
  $|x|$ within $2 (ntaps + chunk)$ inputs. Inputs within 42 dB of the loudest
  in recent chunks are held exactly. After a larger drop, about one chunk of
  outputs keeps fewer bits than float. A NaN or inf makes ntaps outputs NaN.
  The caller gives buffers for the converted coefficients and for a float
  copy of the history.

Asserts
: `ntaps` or the factor 0, a state buffer shorter than `ntaps`, float
  coefficients that are not finite.

Checked
: - Bit for bit against a 128-bit convolution at 16 lengths from 1 to 64
    taps, with $sum |h|$ below, at and far above 2, on random, alternating
    full-scale and constant -1 input; six chunk sizes, blocks cut at
    random, in place, and one sample at a time.
  - Decimators by 1 to 7 and interpolators by 1 to 8 against the same
    reference, decimated or zero-stuffed.
  - {c:func}`fd_fir_f32` against the bound on every output: 7 signals
    (steady noise, bursts 120 dB down, random levels, steps, values near the
    bottom and the top of the float range, clicks) at 5 lengths from 1 to 63
    taps and 4 chunk sizes, worst 0.98 of the bound. NaN and inf make
    exactly ntaps outputs NaN. In place, one sample at a time and after a
    reset.
  - {c:func}`fd_fir_design` against a windowed sinc in double.

## API

```{doxygendefine} FD_FIR_STATE_LEN
```

```{doxygenstruct} FD_FirQ15
```

```{doxygenstruct} FD_FirQ31
```

```{doxygenfunction} fd_fir_init_q15
```

```{doxygenfunction} fd_fir_init_q31
```

```{doxygenfunction} fd_fir_reset_q15
```

```{doxygenfunction} fd_fir_reset_q31
```

```{doxygenfunction} fd_fir_q15
```

```{doxygenfunction} fd_fir_q31
```

```{doxygenfunction} fd_fir_sample_q15
```

```{doxygenfunction} fd_fir_sample_q31
```

### Float

```{doxygenstruct} FD_FirF32
```

```{doxygenfunction} fd_fir_init_f32
```

```{doxygenfunction} fd_fir_reset_f32
```

```{doxygenfunction} fd_fir_f32
```

```{doxygenfunction} fd_fir_sample_f32
```

### Decimators

```{doxygenstruct} FD_FirDecimQ15
```

```{doxygenstruct} FD_FirDecimQ31
```

```{doxygenfunction} fd_fir_decim_init_q15
```

```{doxygenfunction} fd_fir_decim_init_q31
```

```{doxygenfunction} fd_fir_decim_q15
```

```{doxygenfunction} fd_fir_decim_q31
```

### Interpolators

```{doxygendefine} FD_FIR_INTERP_STATE_LEN
```

```{doxygenstruct} FD_FirInterpQ15
```

```{doxygenstruct} FD_FirInterpQ31
```

```{doxygenfunction} fd_fir_interp_init_q15
```

```{doxygenfunction} fd_fir_interp_init_q31
```

```{doxygenfunction} fd_fir_interp_q15
```

```{doxygenfunction} fd_fir_interp_q31
```

### Design

```{doxygenfunction} fd_fir_design
```

```{doxygenfunction} fd_fir_quantize_q15
```

```{doxygenfunction} fd_fir_quantize_q31
```

```{doxygenfunction} fd_fir_gain_q15
```

```{doxygenfunction} fd_fir_gain_q31
```

## Numbers

32 taps, blocks of 64, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.
Errors over 10^6 random samples through a Hamming lowpass.

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_fir_q15` | {cy}`fd_fir_q15, 32 taps x 64` | {cy}`arm_fir_q15, 32 taps x 64` | {acc}`FIR q15, 32 taps; flexdsp` | {acc}`FIR q15, 32 taps; CMSIS-DSP` |
| `fd_fir_q15`, $sum \|h\| >= 2$ | {cy}`fd_fir_q15, gain 2 or more (64-bit)` | | 0.5 | |
| `fd_fir_q31` | {cy}`fd_fir_q31, 32 taps x 64` | {cy}`arm_fir_q31, 32 taps x 64` | {acc}`FIR q31, 32 taps; flexdsp` | {acc}`FIR q31, 32 taps; CMSIS-DSP` |
| `fd_fir_decim_q15` by 4 | {cy}`fd_fir_decim_q15 by 4, 32 taps x 64` | {cy}`arm_fir_decimate_q15 by 4, 32 x 64` | 0.5 |  |
| `fd_fir_interp_q15` by 4 | {cy}`fd_fir_interp_q15 by 4, 32 taps x 64` | {cy}`arm_fir_interpolate_q15 by 4, 32 x 64` | 0.5 |  |
| `fd_fir_sample_q15` | {cy}`fd_fir_sample_q15, 32 taps` |  | 0.5 |  |

CMSIS-DSP: `arm_fir_q15`, `arm_fir_q31`, `arm_fir_decimate_q15`,
`arm_fir_interpolate_q15`. `arm_fir_fast_q15` takes {cy}`arm_fir_fast_q15, 32 taps x 64` cycles ({acc}`FIR q15, 32 taps (fast); CMSIS-DSP` LSB),
`arm_fir_fast_q31` {cy}`arm_fir_fast_q31, 32 taps x 64` ({acc}`FIR q31, 32 taps (fast); CMSIS-DSP` LSB).

Float, the same filter, soft float on the F103. Errors are in ulp of the
exact output, over 10^6 samples of white noise.

| function | cycles | CMSIS-DSP | mean error, ulp | CMSIS-DSP, ulp |
|---|--:|--:|--:|--:|
| `fd_fir_f32` | {cy}`fd_fir_f32, 32 taps x 64` | {cy}`arm_fir_f32, 32 taps x 64` | {acc}`FIR f32, 32 taps; flexdsp; 10^6 samples of white noise, Hamming lowpass @ mean` | {acc}`FIR f32, 32 taps; CMSIS-DSP; 10^6 samples of white noise, Hamming lowpass @ mean` |
| `fd_fir_sample_f32` | {cy}`fd_fir_sample_f32, 32 taps` |  |  |  |

In 300-sample bursts 42 dB apart the mean error is
{acc}`FIR f32, 32 taps; flexdsp; the same in 300-sample bursts 42 dB apart @ mean`
ulp (CMSIS-DSP
{acc}`FIR f32, 32 taps; CMSIS-DSP; the same in 300-sample bursts 42 dB apart @ mean`).
At 120 dB apart it is
{acc}`FIR f32, 32 taps; flexdsp; the same in 300-sample bursts 120 dB apart @ mean`
ulp against
{acc}`FIR f32, 32 taps; CMSIS-DSP; the same in 300-sample bursts 120 dB apart @ mean`:
the quiet outputs after each drop keep fewer bits.

## Example

```{literalinclude} ../examples/fd_fir.c
:language: c
```

## How it works

Init computes $sum |h|$ ({c:func}`fd_fir_gain_q15`). Below 2, every partial sum of a
q15 filter fits 32 bits and of a q31 filter 64 bits, and the narrow kernel
runs; at 2 or more the kernel keeps 64-bit (q31: 96-bit) sums, at about
twice the cycles. An interpolator decides per branch.

The float filter converts its coefficients once, at the smallest exponent
with $sum |h| < 2$, and runs the narrow q31 kernel. Inputs are held as q31
values at one exponent E. E rises at once with a louder chunk and falls
after ntaps - 1 inputs without one. When E changes, the history is
converted again from the float copy, so each held input is rounded once.
Each output is the exact 64-bit sum, rounded once to float.
