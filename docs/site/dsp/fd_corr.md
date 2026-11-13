<p class="fl-header-name">fd_corr.h</p>

# Correlation

Convolution, cross-correlation at any lags, autocorrelation, peak
interpolation and Levinson-Durbin in q15 and q31, and correlation through
q15 FFTs.

Rounding
: Every direct output is an exact sum over 2^(15 + shift) (q31:
  2^(31 + shift)), rounded once (ties up) and saturated. shift goes to 16
  (q31: 32), and shift = ceil(log2 min(na, nb)) rules out saturation.

Lags
: {c:func}`fd_corr_q15` computes any range of lags, negative ones included;
  terms outside either array are zero. All lags: lag0 = 1 - nb,
  nlags = na + nb - 1, the values and order of CMSIS-DSP's
  `arm_correlate_q15`.

Autocorrelation
: {c:func}`fd_autocorr_q15` returns r[k] 2^e with r[0] in [2^30, 2^31): 31
  significant bits at any signal level, the input
  {c:func}`fd_levinson_q31` wants.

Levinson-Durbin
: The order-p predictor x[n] ~ sum a[j - 1] x[n - j], with each step's sums
  exact and rounded once. It reports a matrix that is not positive definite
  and coefficients that pass 2^ashift.

FFT correlation
: {c:func}`fd_corr_fft_q15` correlates two zero-padded blocks in place
  through q15 FFTs. The error is about 45 dB below the output's power, 41 dB
  at worst; on delays with noise down to 0 dB SNR and on chirps it finds the
  same peak lag as the exact sum.

Aliasing
: `y` overlaps neither input.

Asserts
: na or nb 0, shift past 16 (q31: 32), a range past the outputs, a peak
  search past 32768, an order outside 1 to 63, ashift past 30.

Checked
: - Bit for bit against 128-bit sums: every size from 1 x 1 to 12 x 12 with
    four input kinds on each side (random full scale, small, alternating
    full scale, all most negative), nine larger pairs up to 300 x 300, and
    lags with no terms on either side.
  - `make exhaustive`: every size to 16 against exact sums on 10^6 blocks;
    the peak vertex for all 2^32 pairs of neighbour drops.
  - {c:func}`fd_levinson_q31` bit for bit against a 128-bit model on 100000
    random autocorrelations at every ashift, statuses included; its error
    on AR(2) data with poles from 0.3 to 0.99.
  - {c:func}`fd_corr_fft_q15` over 600 trials of delays in noise and chirps:
    SNR above 40 dB and the peak lag of the exact sum every time.

## API

```{doxygenfunction} fd_conv_q15
```

```{doxygenfunction} fd_conv_q31
```

```{doxygenfunction} fd_conv_part_q15
```

```{doxygenfunction} fd_conv_part_q31
```

```{doxygenfunction} fd_corr_q15
```

```{doxygenfunction} fd_corr_q31
```

```{doxygenfunction} fd_corr_fft_q15
```

```{doxygenfunction} fd_autocorr_q15
```

```{doxygenfunction} fd_autocorr_q31
```

```{doxygenfunction} fd_peak_q15
```

```{doxygenfunction} fd_peak_q31
```

```{doxygenfunction} fd_levinson_q31
```

## Numbers

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_conv_q15`, 64 x 64 | 20270 | 123619 | 0.5 | 1 |
| `fd_conv_q15`, 256 x 32 | 32765 | 516899 | 0.5 | 1 |
| `fd_conv_q15`, 64 x 64, both full scale | 46388 | | 0.5 | |
| `fd_conv_q31`, 64 x 64 | 47073 | 49642 | 0.5 | 1 |
| `fd_corr_q15`, 256 x 256, all lags | 614396 | 1738429 | 0.5 | 1 |
| `fd_corr_q15`, 256 x 256, lags 0 to 31 | 70614 | | 0.5 | |
| `fd_corr_q15`, the same on 12-bit data | 32637 | | 0.5 | |
| `fd_corr_q31`, 64 x 64, all lags | 46647 | 51009 | 0.5 | |
| `fd_corr_fft_q15`, 256 x 256 in 512 | 131431 | | 41 dB SNR | |
| `fd_autocorr_q15`, 256, 16 lags | 38605 | | 0.5 | |
| `fd_autocorr_q15`, the same on 12-bit data | 16850 | | exact | |
| `fd_autocorr_q31`, 128, 16 lags | 44395 | | 0.5 | |
| `fd_peak_q15`, 128 | 1325 | | 0.5 (Q16.16) | |
| `fd_levinson_q31`, order 10 | 4909 | 3779 | 7.7 | 82 |

`arm_conv_fast_q15` takes 42252 cycles for 64 x 64 (1 LSB). Levinson errors
are against a `double` recursion on the same autocorrelation, 2000 AR(2)
blocks of 1024 samples with poles from 0.3 to 0.7, in LSB of q31;
CMSIS-DSP rounds every product and divides through a q15 reciprocal.

## Example

```{literalinclude} ../examples/fd_corr.c
:language: c
```

## How it works

Runs of whole windows go through the FIR kernels, four outputs per pass
(q31: three), and so do the ends, with the samples past either end read as
zero. The sums are 32-bit (q31: 64-bit) whenever max |a| sum |b| or
max |b| sum |a| allows it, which a filter with sum |h| < 2 or 12-bit data
always does; otherwise 64-bit (q31: 96-bit).
