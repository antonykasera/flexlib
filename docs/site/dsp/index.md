# flexdsp

Signal processing in q15 and q31 for Cortex-M parts without an FPU, on
flexmath's fixed-point core. C11, no allocation; `flexdsp.mk` brings
flexmath and flexutils with it.

## Conventions

- Samples are `fm_q15`, `fm_q31` or `float`; complex samples are interleaved
  `{re, im}` pairs (`fd_cq15`, `fd_cq31`), the CMSIS-DSP layout.
- Block functions take `(x, y, n)`; `y` may equal an input of the same type
  unless the function says otherwise.
- Results round to nearest (ties up) and saturate, within 0.5 LSB unless
  stated. Filters and correlations round the exact sum once, for any
  coefficients. The FFTs round ties down and never saturate.
- Filters keep their state in buffers the caller passes to init. Init and
  design functions return `FD_Status` or assert; processing functions
  return nothing.
- The kernels are the same C on every Cortex-M and give the same bits on
  each.

## Modules

| header | contents |
|---|---|
| [`fd_types.h`](fd_types.md) | complex types, status codes, bands and windows |
| [`fd_vec.h`](fd_vec.md) | element-wise arithmetic, conversions, statistics, complex vectors |
| [`fd_fir.h`](fd_fir.md) | FIR filters, decimators, interpolators, windowed-sinc design |
| [`fd_biquad.h`](fd_biquad.md) | biquad cascades, RBJ and Butterworth design |
| [`fd_filt.h`](fd_filt.md) | DC blocker, one-pole smoother, moving average, medians |
| [`fd_window.h`](fd_window.md) | eight fixed windows, Kaiser, Gaussian, Tukey, window gains |
| [`fd_tone.h`](fd_tone.md) | Goertzel, NCO, recursive oscillator |
| [`fd_noise.h`](fd_noise.md) | uniform, Gaussian and triangular noise |
| [`fd_fft.h`](fd_fft.md) | complex and real FFTs and inverses, block floating point |
| [`fd_corr.h`](fd_corr.md) | convolution, correlation, autocorrelation, peak, Levinson-Durbin |
| [`fd_lms.h`](fd_lms.md) | LMS, normalised LMS, sign-error LMS |
| [`fd_resample.h`](fd_resample.md) | rational resampling, half-band by 2, CIC decimation |

## Options

`FD_FFT_MAX`
: The largest FFT, a power of two from 4 to 4096; 1024 by default. Up to
  1024 the twiddles come from flexmath's sine table; 2048 and 4096 add a
  4 KB table.

`FD_LUT_IN_RAM`
: With `FD_FFT_MAX` above 1024, puts that table in `.data`. `FM_LUT_IN_RAM`
  moves flexmath's sine table, which the smaller sizes use.

`FX_ASSERT_FAIL(file, line)`, `FX_DISABLE_ASSERT`
: Argument checks trap by default.

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
