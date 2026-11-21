# flexdsp

flexdsp is signal processing for Cortex-M parts without an FPU: arithmetic
on blocks of samples, FIR and biquad filters, windows, tone detection,
noise, FFTs, correlation, adaptive filters and sample-rate conversion. It
works in q15 and q31 on flexmath's fixed-point core, and the FIR, biquad,
FFT, convolution, correlation and sums also come in float, computed on the
same integer kernels so they need no FPU either.

## Conventions

Samples are `fm_q15`, `fm_q31` or `float`. Complex samples are interleaved
`{re, im}` pairs, `fd_cq15`, `fd_cq31` and `fd_cf32`, the layout CMSIS-DSP
uses. Block functions take `(x, y, n)`, and `y` may be the same buffer as an
input of the same type unless the function says otherwise.

Fixed-point results round to nearest, with ties going up, and saturate.
Unless a page says otherwise they are within 0.5 LSB. The filters and
correlations go further: they compute each output as the exact sum of its
products and round once, for any coefficients. The FFTs round with ties
down and never saturate. The float forms give their error in ulp.

Filters keep their state in buffers you pass to their init function, so
nothing allocates. Init and design functions return an `FD_Status` or stop
at an assert; processing functions return nothing. The kernels are the same
C on every Cortex-M and give the same bits on each.

| header | what it is for |
|---|---|
| [`fd_types.h`](fd_types.md) | complex types, status codes, filter bands and windows |
| [`fd_vec.h`](fd_vec.md) | arithmetic, conversions and statistics on blocks, complex vectors |
| [`fd_fir.h`](fd_fir.md) | FIR filters, decimators and interpolators, and their design |
| [`fd_biquad.h`](fd_biquad.md) | biquad cascades for IIR filters and EQs, RBJ and Butterworth design |
| [`fd_filt.h`](fd_filt.md) | DC blocker, one-pole smoother, moving average, medians |
| [`fd_window.h`](fd_window.md) | window functions and their gains |
| [`fd_tone.h`](fd_tone.md) | Goertzel tone detection, NCO, recursive oscillator |
| [`fd_noise.h`](fd_noise.md) | uniform, Gaussian and triangular noise |
| [`fd_fft.h`](fd_fft.md) | complex and real FFTs and their inverses |
| [`fd_corr.h`](fd_corr.md) | convolution, correlation, autocorrelation, peak finding, Levinson-Durbin |
| [`fd_lms.h`](fd_lms.md) | LMS adaptive filters |
| [`fd_resample.h`](fd_resample.md) | rational resampling, half-band filters, CIC decimation |

flexdsp brings flexmath and flexutils with it. Set `FLEXLIB_LIBS := dsp`, as
[Get started](../get-started.md) shows.

## Options

`FD_FFT_MAX`
: The largest FFT size, a power of two from 4 to 4096, 1024 by default. Up
  to 1024 the twiddle factors come from flexmath's sine table. 2048 and 4096
  add a 4 KB table.

`FD_LUT_IN_RAM`
: With `FD_FFT_MAX` above 1024, puts that table in RAM. `FM_LUT_IN_RAM`
  does the same for flexmath's sine table, which the smaller sizes use.

`FX_ASSERT_FAIL(file, line)`, `FX_DISABLE_ASSERT`
: By default a failed argument check traps. Define the first to report it
  your own way, or the second to remove the checks.

```{toctree}
:hidden:

fd_types
fd_vec
fd_fir
fd_biquad
fd_filt
fd_window
fd_tone
fd_noise
fd_fft
fd_corr
fd_lms
fd_resample
```
