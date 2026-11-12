# flexdsp

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexdsp-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

Signal processing for Cortex-M on flexlib's fixed-point core: block
arithmetic, filters, windows and generators in Q15 and Q31. Plain
C11, no allocation.

Cycles are medians for a block of 64 on an STM32F103 at 72 MHz (`-O2`, warm
cache), from [docs/results/f103.md](../docs/results/f103.md). Errors are
against exact references; CMSIS-DSP's are in
[docs/accuracy.md](../docs/accuracy.md), each in its own output format.

## Conventions

- Samples are `fm_q15`, `fm_q31` or `float`; complex samples are interleaved
  `{re, im}` pairs (`fd_cq15`, `fd_cq31`), the CMSIS-DSP layout.
- Block functions take `(x, y, n)`; `y` may equal an input of the same type.
- Results round to nearest (ties up) and saturate: within 0.5 LSB unless
  stated. The FFTs round ties down and never need to saturate (below).
- Programmer errors (a bad shift, `n == 0` where a result is needed) trip
  `FX_ASSERT`.
- The kernels are the same C on every Cortex-M, with the same results on
  each; the M4 / M7 builds don't use the DSP-extension instructions.

## Element-wise (`fd_vec.h`)

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_add_q15` / `fd_add_q31` | 516 / 595 | 539 / 1137 | exact | exact |
| `fd_mul_q15` / `fd_mul_q31` | 611 / 1331 | 667 / 843 | 0.5 LSB | 1 / 2 LSB |
| `fd_scale_q15` / `fd_scale_q31` | 590 / 1280 | 510 / 986 | 0.5 LSB | 1 LSB |
| `fd_shift_q15` (right) | 521 | 380 | 0.5 LSB | truncates |
| `fd_offset_q15`, `fd_negate_q15`, `fd_abs_q15` | 413, 410, 585 | 452, 562, 619 | exact | exact |
| `fd_clip_q15` | 576 | 692 | exact | exact |
| `fd_fill_q15`, `fd_copy_q15` | 144, 214 | 138, 231 | | `memcpy` 918 |

## Conversions

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_q31_to_q15` | 431 | 365 | 0.5 LSB | 1 LSB |
| `fd_q15_to_f32` / `fd_f32_to_q15` | 1401 / 2359 | 4051 / 3283 | exact / 0.5 LSB | |
| `fd_db_q15` / `fd_db_q31` | 3963 | | 0.54 / 0.97 LSB | |

## Statistics

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_sum_q15`, `fd_mean_q15` | 318, 352 | -, 266 | exact, 0.5 LSB | |
| `fd_dot_q15` / `fd_dot_q31` | 654 / 1284 | 670 / 914 | exact / 0.5 LSB | exact / 42 LSB |
| `fd_power_q15` / `fd_power_q31` | 546 / 1205 | 517 / 817 | exact / 0.5 LSB | exact / 41 LSB |
| `fd_rms_q15` / `fd_rms_q31` | 865 / 1592 | 708 / 961 | 0.5 LSB | 179 LSB |
| `fd_var_q15` | 1369 | 838 | 0.5 LSB | 1 LSB |
| `fd_std_q15` / `fd_std_q31` | 1411 / 2291 | 916 | 0.5 LSB | 178 LSB |
| `fd_max_q15`, `fd_absmax_q15` | 559, 850 | 530, 1097 | exact | exact |

q31 dot and power return Q16.48, CMSIS-DSP's format, saturating instead of
wrapping past 2^15. `fd_headroom_q15` / `_q31` return a block's spare sign bits.

## Complex vectors

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_cmul_q15` / `fd_cmul_q31` (32) | 1712 / 2150 | 1300 / 1167 | 0.5 LSB | 2 LSB (Q3.13) |
| `fd_cmag_q15` / `fd_cmag_q31` | 3331 / 7384 | 8612 / 8848 | 0.5 LSB | 1 LSB (Q2.14) |
| `fd_cmag2_q15` | 718 | 958 | exact (UQ2.30) | Q3.13 |
| `fd_cdot_q15` | 1806 | | exact | |

`fd_cmag` returns unsigned UQ1.15 / UQ1.31, so magnitudes up to sqrt 2 need
no saturation.

## FIR filters (`fd_fir.h`)

32 taps, blocks of 64. Every output is the exact sum rounded once, for any
coefficients: init picks 32-bit (q15) or 64-bit (q31) sums when sum |h| < 2,
wider ones otherwise. `h[0]` multiplies the newest sample.

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_fir_q15` | 8078 | 19863 (`fast` 13941) | 0.5 LSB | 1 LSB |
| `fd_fir_q15`, sum \|h\| >= 2 | 18134 | | 0.5 LSB | |
| `fd_fir_q31` | 17836 | 16593 (`fast` 22446) | 0.5 LSB | 1 LSB (`fast` 17) |
| `fd_fir_decim_q15` by 4 | 3686 | 4854 | 0.5 LSB | |
| `fd_fir_interp_q15` by 4 | 12522 | 20806 | 0.5 LSB | |
| `fd_fir_sample_q15` | 453 | | 0.5 LSB | |

The state holds `ntaps - 1` history samples plus a chunk of any size
(`FD_FIR_STATE_LEN`); a block is processed in chunk-sized pieces, and the
history moves once per chunk. Decimators take any `n` and return the outputs
written. `fd_fir_design` builds windowed-sinc lowpass, highpass, bandpass and
bandstop filters in float (rectangular, Hann, Hamming, Blackman,
Blackman-Harris), and `fd_fir_quantize_q15` / `_q31` round them.

## Biquads (`fd_biquad.h`)

Four sections, blocks of 64 (an 8th-order Butterworth lowpass). DF1 with the
CMSIS coefficient order and signs, Q2.30 / Q2.14. Each section's output is
its exact sum rounded once, for any coefficients; large `b` gains use a shift
on the `b` alone, so the feedback coefficients keep full precision.

| function | cycles | CMSIS-DSP | error, fc 0.01 | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_biquad_q15` | 6120 | 13291 (`fast` 8778) | 96 LSB | 355 LSB |
| `fd_biquad_q31` | 14200 | 11883 (`fast` 16631) | 80 LSB | 349 LSB |
| `fd_biquad_q31` with error feedback | 18052 | `32x64` 25739 | 5 LSB | `32x64` 1.7 LSB |

Errors are the recursion's, against the same coefficients in double, over
10^6 samples of noise ([docs/accuracy.md](../docs/accuracy.md)). At fc 5e-4
plain q31 reaches 8170 LSB and error feedback 115 (CMSIS: 110000; `32x64`:
1.6).

Shelves and peaks, most of an audio EQ, have |b0| + |b1| + |b2| + |a1| + |a2|
of 4 or more. For those sections the b and a parts are summed apart, each
exact in 64 bits (q31) or 32 bits (q15), and added with an overflow check;
only sections whose stored b or a alone reach 4 (strong shelf boosts,
hand-made coefficients) take 96 / 64-bit sums. A four-section 48 kHz EQ (low
shelf, two peaks, high shelf):

| EQ, 4 sections x 64 | q15 | q31 | q31 with error feedback |
|---|--:|--:|--:|
| `bshift` 0 | 8070 | 16815 | 17831 |
| `bshift` 1 (a +12 dB high shelf) | 9908 | 22818 | |

`fd_biquad_design` returns RBJ sections (lowpass, highpass, bandpass, notch,
peak, low and high shelf, allpass) with the poles as deviations from z = 1,
which keeps low-frequency poles exact in Q2.30; `fd_biquad_butter` gives
even-order Butterworth cascades; `fd_biquad_quantize_q31` / `_q15` round
them, pick the `b` shift and report sections made unstable by rounding.

## Small filters (`fd_filt.h`)

| function | cycles (64 samples) | |
|---|--:|---|
| `fd_dcblock_q15` / `_q31` | 1064 / 2027 | `y = x - x1 + a y1` |
| `fd_onepole_q15` / `_q31` | 796 / 1404 | `y += a (x - y)`; `fd_onepole_coef` puts -3 dB at fc |
| `fd_movavg_q15`, n = 32 / 30 | 1264 / 2429 | exact running sum; a power of two divides by shifting |
| `fd_movavg_q31`, n = 32 | 2309 | |
| `fd_median5` | 30 | branchless, also `fd_median3` |

## Windows (`fd_window.h`)

Generated at run time for any length, symmetric (filter design) or periodic
(spectra), from flexmath's cosine on exact binary angles. Apply one with
`fd_mul_q15` / `_q31`.

| function | cycles (256) | CMSIS-DSP | error |
|---|--:|--:|--:|
| `fd_window_q15` Hann | 23464 | `arm_hanning_f32` 396495 | 0.5 LSB |
| `fd_window_q31` Blackman-Harris | 54203 | | 3 LSB |
| `fd_window_kaiser_q15`, beta 8 | 750638 | | 0.52 LSB |

Types: rectangular, Hann, Hamming, Blackman, Blackman-Harris, Nuttall,
flat-top, Bartlett: checked at every length up to 4096 (q31 at most 2.74 LSB).
Kaiser, Gaussian and Tukey take a float parameter and are computed in float
(q31 within 1.5e-6). `fd_window_gains_q15` / `_q31` give the coherent and
power gains for scaling spectra.

## Tones and noise (`fd_tone.h`, `fd_noise.h`)

| function | cycles (64 samples) | |
|---|--:|---|
| `fd_goertzel_q15` / `_q31` | 1374 / 1921 | one DFT term at any frequency; `fd_goertzel_power` exact from the state |
| `fd_nco_q15` sin | 5303 | 0.5 LSB of the 32-bit phase; a 16-bit phase into `fm_sin_q15` is 2400 but 1.6 LSB |
| `fd_nco_q31` sin + cos | 7067 | `fm_sincos_q31` (1.11 LSB), linear chirps, complex output |
| `fd_osc_q15` sin / `fd_osc_q31` sin + cos | 2694 / 3925 | rotation per sample; amplitude within 7e-8, phase drift 5.5e-5 rad over 10^6 samples |
| `fd_noise_uniform_q15` | 631 | |
| `fd_noise_gauss_q15` | 14112 | Box-Muller, tails to 6.6 sigma; a sum of 12 uniforms is 7760 but cut at 6 sigma |
| `fd_noise_tpdf_q15` | 1203 | triangular dither of +-2^bits LSB |

## Transforms (`fd_fft.h`)

Complex FFTs of any power of two from 2 to `FD_FFT_MAX` points, in place on
`fd_cq15` / `fd_cq31`; real FFTs of 4 to `FD_FFT_MAX` points as a half-size
complex one plus a split pass; and the inverses of both. `FD_FFT_MAX` is 1024
by default, with the twiddles taken from flexmath's sine table (no table of
their own); 2048 or 4096 add a 4 KB table. Radix-4, decimation in time.

Block floating point: before each stage the block's headroom is measured, and
the stage scales by exactly what its growth needs, left as well as right, so a
quiet input keeps its bits. The transform is `x[k] 2^e` in integer units, with
`e` the return value. Every stage output is its exact value rounded once (to
nearest, ties down), and nothing saturates or wraps for any input. An all-zero
block gives zeros and `e = 0`.

| transform | cycles | CMSIS-DSP | SNR, white noise | CMSIS-DSP |
|---|--:|--:|--:|--:|
| `fd_cfft_q15`, 256 | 32433 | 32665 | 72.4 dB | 60.1 dB |
| `fd_cfft_q15`, 1024 | 157075 | 159716 | 68.3 dB | 54.0 dB |
| `fd_cfft_q31`, 256 | 59677 | 36223 | 167.8 dB | 152.6 dB |
| `fd_cfft_q31`, 1024 | 295942 | 182148 | 164.4 dB | 147.5 dB |
| `fd_rfft_q15`, 512 | 40107 | 109341 | 69.8 dB | 57.1 dB |
| `fd_rfft_q31`, 512 | 81801 | | 166.7 dB | 150.8 dB |
| `fd_icfft_q15`, 256 / `fd_irfft_q15`, 512 | 33849 / 39724 | | | |
| `fd_fft_db_q15`, 128 bins | 15160 | | 0.5003 LSB | |

SNR is against the exact DFT; white noise at full scale, 40 blocks
([docs/accuracy.md](../docs/accuracy.md) has tones, two tones and quiet input).
CMSIS-DSP scales by a fixed 1/4 per stage, so a quiet signal loses its low
bits: white noise at -40 dB keeps 70.4 dB here at 256 points (CMSIS-DSP 20.1),
and q31 keeps 164.9 dB at 1024 (107.5). The q31 butterflies sum each output
exactly in 64 bits; CMSIS-DSP keeps the truncated high word of each product,
which is why it is faster there. Rounding ties down in the stages without
twiddles leaves a mean error of -1.1 LSB at DC at 256 points (-1.9 at 1024;
CMSIS-DSP's truncation, -5 and -6 in its format).

Layouts and scaling:

- Complex: `fd_cq15` / `fd_cq31` arrays, bin k at `x[k]`.
- Real: n samples in, n/2 complex values out, in place: `x[0]` = Re X[0],
  `x[1]` = Re X[n/2] (both bins are real), then `x[2k]`, `x[2k + 1]` = X[k] for
  k = 1 .. n/2 - 1. The buffer must be 4-byte aligned. CMSIS-DSP's
  `arm_rfft_q15` writes X[0] .. X[n/2] to a second buffer (with zero imaginary
  parts at 0 and n/2), scaled by 1/n.
- `fd_fft_rescale_q15(x, n, e, to)` moves bins to exponent `to`, rounded and
  saturated; `to = log2 n` gives CMSIS-DSP's X / n.
- Magnitude and power are `fd_cmag_*` and `fd_cmag2_*`. `fd_fft_db_q15` /
  `_q31` give 10 log10 |X|^2 with the exponent folded in, 0 dB at |X| = full
  scale.

A spectrum in dB from 512 samples:

```c
static fm_q15 w[512], x[512] __attribute__((aligned(4)));
fm_q16 db[256];

fd_window_q15(w, 512U, FD_WIN_HANN, true);       /* once */
fd_mul_q15(x, w, x, 512U);
int32_t e = fd_rfft_q15(x, 512U);
fd_fft_db_q15((fd_cq15 *)(void *)x, e, db, 256U); /* db[0] mixes DC and n/2 */
```

A full-scale sine reads 20 log10(n/2), less the window's coherent gain
(`fd_window_gains_q15`), above 0 dB.

## Convolution and correlation (`fd_corr.h`)

`fd_conv_q15` / `_q31` give all `na + nb - 1` outputs, `fd_conv_part_*` a range
of them, and `fd_corr_q15` / `_q31` any range of lags, negative ones included
(all of them: `lag0 = 1 - nb`, `nlags = na + nb - 1`; CMSIS-DSP's
`arm_correlate_q15` puts the same values in the same order). Every output is
the exact sum rounded once and saturated; `shift` divides by a further power of
two, and `shift = ceil(log2 min(na, nb))` rules out saturation. Runs of whole
windows go through the FIR kernels, four outputs per pass (q31: three), and so
do the ends, with the samples past either end read as zero. The sums are
32-bit (q31: 64-bit) whenever max |a| sum |b| or max |b| sum |a| allows it,
which a filter with sum |h| < 2 or 12-bit ADC data always does; otherwise
64-bit (q31: 96-bit).

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_conv_q15`, 64 x 64 | 20270 | 123619 (`fast` 42252) | 0.5 LSB | 1 LSB |
| `fd_conv_q15`, 256 x 32 | 32765 | 516899 | 0.5 LSB | 1 LSB |
| `fd_conv_q15`, 64 x 64, both full scale | 46388 | | 0.5 LSB | |
| `fd_conv_q31`, 64 x 64 | 47073 | 49642 | 0.5 LSB | 1 LSB |
| `fd_corr_q15`, 256 x 256, all lags | 614396 | 1738429 | 0.5 LSB | 1 LSB |
| `fd_corr_q15`, 256 x 256, lags 0..31 | 70614 | | 0.5 LSB | |
| the same on 12-bit data | 32637 | | 0.5 LSB | |
| `fd_corr_q31`, 64 x 64, all lags | 46647 | 51009 | 0.5 LSB | |
| `fd_corr_fft_q15`, 256 x 256 in 512 | 131431 | | 41 dB SNR | |
| `fd_autocorr_q15`, 256, 16 lags | 38605 | | 0.5 LSB | |
| the same on 12-bit data | 16850 | | exact | |
| `fd_autocorr_q31`, 128, 16 lags | 44395 | | 0.5 LSB | |
| `fd_peak_q15`, 128 | 1325 | | 0.5 LSB (Q16.16) | |
| `fd_levinson_q31`, order 10 | 4909 | 3779 | 7.7 LSB | 82 LSB |

- `fd_corr_fft_q15` correlates two zero-padded blocks through q15 FFTs, in
  place: 4.7 times faster than the direct sum at 256 x 256, with the error
  about 45 dB (41 at worst) below the output's power instead of exact. On
  delays with noise down to 0 dB SNR and on chirps it finds the same peak lag
  as the exact sum.
- `fd_autocorr_q15` / `_q31` return `r[k] 2^e` with `r[0]` in [2^30, 2^31):
  31 significant bits at any signal level, the input `fd_levinson_q31` wants.
- `fd_peak_q15` / `_q31` return the index of the largest value moved to the
  vertex of the parabola through it and its neighbours, in Q16.16.
- `fd_levinson_q31` gives the predictor `x[n] ~ sum a[j - 1] x[n - j]`, `a` in
  Q(31 - `ashift`), the reflection coefficients and the remaining error power;
  each step's sums are exact and rounded once. Its error is against a `double`
  recursion on the same autocorrelation (AR(2) data, poles 0.3-0.7); CMSIS-DSP
  rounds every product and divides through a q15 reciprocal.

## Adaptive filters (`fd_lms.h`)

LMS in q15 and q31, plain (`w += mu e x`), normalised (`mu e x / x'x`) and
sign-error (`mu sign(e) x`), 32 taps, blocks of 64. The weights are Q30 in 32
bits for both formats, so small updates still move them, and the update for
one sample runs in the same pass over the taps as the next sample's output.
Outputs are exact sums rounded once and saturated; the normalised step is an
exact division, once per sample.

| function | cycles | CMSIS-DSP |
|---|--:|--:|
| `fd_lms_q15` | 35585 | 42696 |
| `fd_lms_q15` normalised | 40057 | 48624 |
| `fd_lms_q15` sign-error | 35550 | |
| `fd_lms_q31` | 64066 | 71613 |
| `fd_lms_q31` normalised | 80848 | |

Identifying a 16-tap system with noise 60 dB down at mu = 2^-10, the weights
end within the error of a `double` LMS on the same data (42 dB below the
system's norm after 60000 samples); CMSIS-DSP's q15 LMS truncates its
updates, drifts, and ends 0.5 dB *above* the norm
([docs/accuracy.md](../docs/accuracy.md)). `fd_lms_coefs_q15` / `_q31` read
the taps back.

## Sample-rate conversion (`fd_resample.h`)

`fd_resample_q15` / `_q31` change the rate by l / m with an FIR prototype at l
times the input rate (gain l, as for `fd_fir_interp`), split into l branches.
Only the kept outputs are computed: each is its branch's exact sum rounded
once, the same bits as interpolating by l and keeping every m-th output.
Outputs l apart share a branch, so when a block holds at least four per branch
they go through the decimator kernel four at a time; otherwise one by one
(48 kHz from 44.1 kHz, 160 / 147). Any block length; the function returns how
many outputs it wrote. `fd_resample_design` builds the prototype as a
windowed-sinc lowpass at a fraction of the lower Nyquist.

`fd_halfband_decim_*` / `fd_halfband_interp_*` change the rate by 2 with a
half-band lowpass (4k - 1 taps, the centre 1/2, the other even offsets zero):
only the 2k nonzero taps are stored and multiplied, and the centre is an exact
add (the interpolator's odd outputs are delayed inputs). Same rounding, same
bits as the full-length filter through `fd_fir_decim`. `fd_halfband_design`
makes one with exactly zero taps where it should.

`fd_cic_decim_q15` / `_q31` average and decimate by `ratio` with `order` (1 to
6) integrators and combs, adds and subtracts only. The registers are sized
from the input width you give (`in_bits`): 32 bits when the result fits, which
12-bit ADC data does up to order 3 by 101 or order 2 by 1024, else 64. The sum
is exact and comes out as q31 with the bits the averaging adds kept, rounded
once when it is wider than 32 bits; `fd_cic_gain` gives the remaining gain,
ratio^order over a power of two, and `fd_cic_comp_design` the FIR that
flattens the passband after it. Each decimator is about 4 KB of code (a loop
per order and width).

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_resample_q15` 3 / 2, 24 taps, 64 in | 6577 | 18188 | 0.5 LSB | 1 LSB |
| `fd_resample_q31` 3 / 2, 24 taps, 64 in | 12347 | 19674 | 0.5 LSB | 1 LSB |
| `fd_resample_q15` 160 / 147, 1280 taps, 64 in | 7923 | | 0.5 LSB | |
| `fd_halfband_decim_q15`, 31 taps, 64 in | 2988 | | 0.5 LSB | |
| `fd_halfband_decim_q31`, 31 taps, 64 in | 6075 | | 0.5 LSB | |
| `fd_halfband_interp_q15`, 31 taps, 64 in | 5293 | | 0.5 LSB | |
| `fd_halfband_interp_q31`, 31 taps, 64 in | 11738 | | 0.5 LSB | |
| `fd_cic_decim_q15` order 3 by 64, 12-bit, 256 in | 2012 | | exact | |
| the same on 16-bit input (64-bit registers) | 2932 | | 0.5 LSB | |
| `fd_cic_decim_q15` order 5 by 16, 12-bit, 256 in | 4688 | | exact | |

- CMSIS-DSP has no rational resampler; its rows interpolate by 3 with
  `arm_fir_interpolate_*` and keep every second output, computing three
  outputs for every two kept. Its q31 interpolator wraps past full scale.
- The half-band filters take half the time of `fd_fir_decim` / `fd_fir_interp`
  by 2 on the same 31 taps (6386 and 10384).
- 160 / 147 is 124 cycles per output with 8 taps per branch: 8% of the F103
  for 48 kHz out.
