# flexdsp

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexdsp-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

flexdsp is signal processing for Cortex-M parts without an FPU: arithmetic
on blocks of samples, FIR and biquad filters, windows, tone detection, noise,
FFTs, correlation, adaptive filters and sample-rate conversion. It works in
Q15 and Q31 on flexmath's fixed-point core, and the FIR, biquad, FFT,
convolution, correlation and sums also come in float, computed on the same
integer kernels so they need no FPU either. Plain C11, no allocation.

Here a 31-tap lowpass at 0.1 cycles per sample is fed DC plus a tone at half
the sample rate:

```c
#include <stdio.h>

#include "fd_fir.h"

#define TAPS 31U

int main(void) {
  /* lowpass at 0.1 cycles per sample, Hamming window */
  float hf[TAPS];
  fm_q15 h[TAPS], st[FD_FIR_STATE_LEN(TAPS, 64U)];
  if (fd_fir_design(hf, TAPS, FD_LOWPASS, 0.1f, 0.0f, FD_WIN_HAMMING) != FD_OK ||
      fd_fir_quantize_q15(hf, h, TAPS) != FD_OK) {
    return 1;
  }
  FD_FirQ15 f;
  fd_fir_init_q15(&f, h, TAPS, st, FD_FIR_STATE_LEN(TAPS, 64U));

  /* DC 8192 plus a Nyquist tone of 8192: the filter keeps the DC */
  fm_q15 x[64], y[64];
  for (uint32_t i = 0U; i < 64U; i++) {
    x[i] = (i & 1U) != 0U ? 16384 : 0;
  }
  fd_fir_q15(&f, x, y, 64U);

  /* what is left of the tone: (y[62] - y[63]) / 2, about 4.5 LSB */
  printf("%d %d\n", y[62], y[63]);
  return y[62] - 8192 < 10 && 8192 - y[63] < 10 ? 0 : 1;
}
```

Which prints

```text
8196 8187
```

The input alternates 0 and 16384: a DC level of 8192 plus a tone of 8192 at
the highest frequency there is. The filter keeps the DC and removes the
tone. What is left of it, half the difference of two outputs, is 4.5 LSB,
about 65 dB down.

Cycles below are medians for a block of 64 on an STM32F103 at 72 MHz
(`-O2`, warm cache). The tables show the main cases, and
[docs/results/f103.md](../docs/results/f103.md) has every case. Errors are
against exact references. CMSIS-DSP's are in
[docs/accuracy.md](../docs/accuracy.md), each in its own output format.

## Conventions

Samples are `fm_q15`, `fm_q31` or `float`. Complex samples are interleaved
`{re, im}` pairs (`fd_cq15`, `fd_cq31`, `fd_cf32`), the layout CMSIS-DSP
uses. Block functions take `(x, y, n)`, and `y` may be the same buffer as an
input of the same type.

Results round to nearest, with ties going up, and saturate, so they are
within 0.5 LSB unless a function says otherwise. The filters and
correlations compute each output as the exact sum of its products and round
once, for any coefficients. The FFTs round ties down and never need to
saturate, as described below. Programmer errors, such as a bad shift or an
empty block where a result is needed, stop at `FX_ASSERT`. The kernels are
the same C on every Cortex-M and give the same results on each. The M4 and
M7 builds do not use the DSP-extension instructions.

## Element-wise (`fd_vec.h`)

Scaling, adding, multiplying, clipping and copying whole blocks. They round
and saturate where plain C would truncate or wrap.

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_add_q15` / `fd_add_q31` | 516 / 595 | 539 / 1137 | exact | exact |
| `fd_mul_q15` / `fd_mul_q31` | 611 / 1331 | 667 / 843 | 0.5 LSB | 1 / 2 LSB |
| `fd_scale_q15` / `fd_scale_q31` | 590 / 1280 | 510 / 986 | 0.5 LSB | 1 LSB |
| `fd_shift_q15` (right) | 521 | 380 | 0.5 LSB | truncates |
| `fd_offset_q15`, `fd_negate_q15`, `fd_abs_q15` | 413, 410, 585 | 452, 562, 619 | exact | exact |
| `fd_clip_q15` | 576 | 692 | exact | exact |
| `fd_fill_q15`, `fd_copy_q15` | 144, 214 | 138, 231 | | `memcpy` 918 |
| `fd_add_f32` / `fd_mul_f32` / `fd_abs_f32` | 4121 / 2861 / 410 | 4120 / 2860 / 404 | one rounding / exact | the same |

## Conversions

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_q31_to_q15` | 431 | 365 | 0.5 LSB | 1 LSB |
| `fd_q15_to_f32` / `fd_f32_to_q15` | 1401 / 2359 | 4051 / 3283 | exact / 0.5 LSB | |
| `fd_db_q15` / `fd_db_q31` | 3963 | | 0.54 / 0.97 LSB | |

## Statistics

The sums, the q15 dot product and power are exact. The RMS, variance and
standard deviation are computed from exact sums and rounded once.

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_sum_q15`, `fd_mean_q15` | 318, 352 | -, 266 | exact, 0.5 LSB | |
| `fd_dot_q15` / `fd_dot_q31` | 654 / 1284 | 670 / 914 | exact / 0.5 LSB | exact / 42 LSB |
| `fd_power_q15` / `fd_power_q31` | 546 / 1205 | 517 / 817 | exact / 0.5 LSB | exact / 41 LSB |
| `fd_rms_q15` / `fd_rms_q31` | 865 / 1592 | 708 / 961 | 0.5 LSB | 179 LSB |
| `fd_var_q15` | 1362 | 838 | 0.5 LSB | 1 LSB |
| `fd_std_q15` / `fd_std_q31` | 1411 / 2291 | 916 | 0.5 LSB | 178 LSB |
| `fd_max_q15`, `fd_absmax_q15` | 559, 850 | 530, 1097 | exact | exact |
| `fd_dot_f32` / `fd_power_f32` | 4504 / 3911 | 6427 / 6318 | 0.5 ulp | 1.7e4 / 7.5 ulp |
| `fd_sum_f32`, `fd_mean_f32` | 3597, 3799 | -, 3896 | 0.5 ulp | -, 1.3e5 ulp |
| `fd_rms_f32` | 4176 | 7033 | 0.84 ulp | 4.3 ulp |

The q31 dot product and power return Q16.48, CMSIS-DSP's format, but
saturate instead of wrapping past 2^15. `fd_headroom_q15` / `_q31` return a
block's spare sign bits. The f32 sums add up exactly in 64 bits before one
rounding, so a long sum does not lose accuracy as it goes. Their errors are
for 64 values from 2^-10 to 2^10.

## Complex vectors

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_cmul_q15` / `fd_cmul_q31` (32) | 1712 / 2150 | 1300 / 1167 | 0.5 LSB | 2 LSB (Q3.13) |
| `fd_cmag_q15` / `fd_cmag_q31` | 3331 / 7384 | 8612 / 8848 | 0.5 LSB | 1 LSB (Q2.14) |
| `fd_cmag2_q15` | 718 | 958 | exact (UQ2.30) | Q3.13 |
| `fd_cdot_q15` | 1806 | | exact | |

`fd_cmag` returns unsigned UQ1.15 / UQ1.31, so a magnitude up to sqrt 2 fits
without saturating.

## FIR filters (`fd_fir.h`)

32 taps, blocks of 64. Every output is the exact sum of its products,
rounded once, for any coefficients. When the coefficients' |values| add up
to less than 2, init picks the fast 32-bit (q15) or 64-bit (q31) sums.
Filters with more gain get wider sums, with the same exact results. `h[0]`
multiplies the newest sample.

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_fir_q15` | 8078 | 19863 (`fast` 13941) | 0.5 LSB | 1 LSB |
| `fd_fir_q15`, sum \|h\| >= 2 | 18134 | | 0.5 LSB | |
| `fd_fir_q31` | 17836 | 16593 (`fast` 22446) | 0.5 LSB | 1 LSB (`fast` 17) |
| `fd_fir_decim_q15` by 4 | 3686 | 4854 | 0.5 LSB | |
| `fd_fir_interp_q15` by 4 | 12522 | 20806 | 0.5 LSB | |
| `fd_fir_f32` | 22735 | 180335 (soft float) | mean 0.37 ulp | mean 4.4 ulp |

You pass a state buffer of `FD_FIR_STATE_LEN` samples: the `ntaps - 1`
history samples plus a chunk of any size. A block runs in chunk-sized
pieces, and the history moves once per chunk, so a larger chunk is faster
and the results do not change. Decimators take any `n` and return the
outputs written. `fd_fir_design` builds windowed-sinc lowpass, highpass,
bandpass and bandstop filters in float, and `fd_fir_quantize_q15` / `_q31`
round them.

`fd_fir_f32` runs the q31 kernel on block floating point, with one rounding
per output (the bound is in `fd_fir.h`). Inputs within 42 dB of the loudest
in recent chunks are held exactly. After a larger drop, about a chunk of
outputs keeps fewer bits than float would. The errors above are on white
noise, and `docs/accuracy.md` has signals in bursts.

## Biquads (`fd_biquad.h`)

A cascade of biquads builds any IIR filter, from a Butterworth lowpass to
the shelves and peaks of an audio EQ, with far fewer multiplies than a FIR.
These are four sections, an 8th-order Butterworth lowpass, on blocks of 64.
They are DF1 with CMSIS-DSP's coefficient order and signs, in Q2.30 / Q2.14.
Each section's output is its exact sum rounded once, for any coefficients. A
large gain is a shift on the `b` coefficients alone, so the feedback keeps
full precision.

| function | cycles | CMSIS-DSP | error, fc 0.01 | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_biquad_q15` | 6120 | 13291 (`fast` 8778) | 96 LSB | 355 LSB |
| `fd_biquad_q31` | 14200 | 11883 (`fast` 16631) | 80 LSB | 349 LSB |
| `fd_biquad_q31` with error feedback | 18052 | `32x64` 25739 | 5 LSB | `32x64` 1.7 LSB |
| `fd_biquad_f32` | 57635 | `df2T` 113574 | mean 207 ulp | mean 574 ulp |

The errors are the recursion's, against the same coefficients in double,
over 10^6 samples of noise ([docs/accuracy.md](../docs/accuracy.md)). The
f32 cascade holds its coefficients and history unpacked, and rounds each
section's exact sum once. At a corner of 5e-4, plain q31 reaches 8170 LSB
and error feedback 115 (CMSIS-DSP: 110000, and 1.6 for its `32x64` form).

Shelves and peaks, most of an audio EQ, have a sum
`|b0| + |b1| + |b2| + |a1| + |a2|` of 4 or more. For those sections the b
and a parts are summed apart, each exact in 64 bits (q31) or 32 bits (q15),
and added with an overflow check. Only sections whose stored b or a alone reach 4, such as strong shelf
boosts or hand-made coefficients, need 96 / 64-bit sums. A four-section
48 kHz EQ (low shelf, two peaks, high shelf):

| EQ, 4 sections x 64 | q15 | q31 | q31 with error feedback |
|---|--:|--:|--:|
| `bshift` 0 | 8070 | 16815 | 17831 |
| `bshift` 1 (a +12 dB high shelf) | 9908 | 22818 | |

`fd_biquad_design` returns RBJ sections (lowpass, highpass, bandpass, notch,
peak, low and high shelf, allpass), holding the poles as offsets from z = 1
so that low-frequency poles stay exact in Q2.30. `fd_biquad_butter` gives
even-order Butterworth cascades. `fd_biquad_quantize_q31` / `_q15` round
them, pick the `b` shift and report sections that rounding made unstable.

## Small filters (`fd_filt.h`)

For the everyday jobs: removing a DC offset, smoothing a reading, averaging
the last few samples, dropping a spike.

| function | cycles (64 samples) | |
|---|--:|---|
| `fd_dcblock_q15` / `_q31` | 1064 / 2027 | `y = x - x1 + a y1` |
| `fd_onepole_q15` / `_q31` | 796 / 1404 | `y += a (x - y)`; `fd_onepole_coef` puts -3 dB at fc |
| `fd_movavg_q15`, n = 32 / 30 | 1264 / 2429 | exact running sum; a power of two divides by shifting |
| `fd_movavg_q31`, n = 32 | 2309 | |
| `fd_median5` | 30 | branchless, also `fd_median3` |

## Windows (`fd_window.h`)

A window tapers a block before an FFT, so a tone does not smear across the
spectrum. These are generated at run time for any length, symmetric for
filter design or periodic for spectra, from flexmath's cosine on exact
binary angles. Apply one with `fd_mul_q15` / `_q31`.

| function | cycles (256) | CMSIS-DSP | error |
|---|--:|--:|--:|
| `fd_window_q15` Hann | 23464 | `arm_hanning_f32` 396495 | 0.5 LSB |
| `fd_window_q31` Blackman-Harris | 54203 | | 3 LSB |
| `fd_window_kaiser_q15`, beta 8 | 750638 | | 0.52 LSB |

Rectangular, Hann, Hamming, Blackman, Blackman-Harris, Nuttall, flat-top and
Bartlett are checked at every length up to 4096 (q31 at most 2.74 LSB).
Kaiser, Gaussian and Tukey take a float parameter and are computed in float
(q31 within 1.5e-6). `fd_window_gains_q15` / `_q31` give the coherent and
power gains you need to read levels off a windowed spectrum.

## Tones and noise (`fd_tone.h`, `fd_noise.h`)

The Goertzel detector measures one frequency, a DTMF digit or a pilot tone,
far more cheaply than an FFT. The NCO and the recursive oscillator generate
sine and cosine, and the noise generators make test signals and dither.

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
`fd_cq15` / `fd_cq31`, real FFTs of 4 to `FD_FFT_MAX` points through a
half-size complex one and a split pass, and the inverses of both.
`FD_FFT_MAX` is 1024 by default, with the twiddles taken from flexmath's sine
table, so they cost no table of their own. 2048 or 4096 add a 4 KB table.
The transforms are radix-4, decimation in time.

A fixed-point FFT has to keep its values from overflowing as they grow
through the stages. These use block floating point: before each stage they
measure the block's headroom and scale by exactly what that stage needs,
down as well as up, so a quiet input keeps its bits. The transform returns
the exponent e, and the result is `x[k] 2^e` in integer units. Every stage
output is its exact value rounded once, to nearest with ties down, and
nothing saturates or wraps for any input. An all-zero block gives zeros and
`e = 0`.

| transform | cycles | CMSIS-DSP | SNR, white noise | CMSIS-DSP |
|---|--:|--:|--:|--:|
| `fd_cfft_q15`, 256 | 32433 | 32665 | 72.4 dB | 60.1 dB |
| `fd_cfft_q15`, 1024 | 157075 | 159716 | 68.3 dB | 54.0 dB |
| `fd_cfft_q31`, 256 | 59677 | 36223 | 167.8 dB | 152.6 dB |
| `fd_rfft_q15`, 512 | 40107 | 109341 | 69.8 dB | 57.1 dB |
| `fd_rfft_q31`, 512 | 81801 | | 166.7 dB | 150.8 dB |
| `fd_icfft_q15`, 256 / `fd_irfft_q15`, 512 | 33849 / 39724 | | | |
| `fd_fft_db_q15`, 128 bins | 15160 | | 0.5003 LSB | |
| `fd_cfft_f32`, 256 | 85313 | 418737 | 151.8 dB | 140.5 dB |
| `fd_rfft_f32`, 512 | 107463 | 604927 | 151.7 dB | 139.4 dB |

SNR is against the exact DFT, on full-scale white noise over 40 blocks
([docs/accuracy.md](../docs/accuracy.md) has tones, two tones and quiet
input). CMSIS-DSP scales by a fixed 1/4 per stage, so a quiet signal loses
its low bits: white noise at -40 dB keeps 70.4 dB here at 256 points
(CMSIS-DSP 20.1), and q31 keeps 164.9 dB at 1024 (107.5). The q31 butterflies
sum each output exactly in 64 bits, where CMSIS-DSP keeps the truncated high
word of each product, which is why CMSIS-DSP is faster there. Rounding ties
down in the stages without twiddles leaves a mean error of -1.1 LSB at DC at
256 points (-1.9 at 1024, against -5 and -6 for CMSIS-DSP's truncation in its
format).

Layouts and scaling:

- Complex: `fd_cq15` / `fd_cq31` arrays, bin k at `x[k]`.
- Real: n samples in, n/2 complex values out, in place: `x[0]` = Re X[0],
  `x[1]` = Re X[n/2] (both bins are real), then `x[2k]`, `x[2k + 1]` = X[k]
  for k = 1 .. n/2 - 1. The buffer must be 4-byte aligned. CMSIS-DSP's
  `arm_rfft_q15` writes X[0] .. X[n/2] to a second buffer, with zero
  imaginary parts at 0 and n/2, scaled by 1/n.
- `fd_fft_rescale_q15(x, n, e, to)` moves bins to exponent `to`, rounded and
  saturated. `to = log2 n` gives CMSIS-DSP's X / n.
- Magnitude and power are `fd_cmag_*` and `fd_cmag2_*`. `fd_fft_db_q15` /
  `_q31` give 10 log10 |X|^2 with the exponent folded in, 0 dB at |X| = full
  scale.
- Float: `fd_cfft_f32` / `fd_rfft_f32` and the inverses run the q31
  transforms on block floating point, with a q31 work buffer of n values
  that you pass beside x.

A spectrum in dB from 512 samples:

```c
static fm_q15 w[512], x[512] __attribute__((aligned(4)));
fm_q16 db[256];

fd_window_q15(w, 512U, FD_WIN_HANN, true);       /* once */
fd_mul_q15(x, w, x, 512U);
int32_t e = fd_rfft_q15(x, 512U);
fd_fft_db_q15((fd_cq15 *)(void *)x, e, db, 256U); /* db[0] mixes DC and n/2 */
```

A full-scale sine reads 20 log10(n/2) dB, less the window's coherent gain
(`fd_window_gains_q15`).

## Convolution and correlation (`fd_corr.h`)

Correlation finds the delay between two recordings, or a known pattern such
as a sync word inside a longer capture. `fd_conv_q15` / `_q31` give all
`na + nb - 1` outputs, `fd_conv_part_*` a range of them, and `fd_corr_q15` /
`_q31` any range of lags, negative ones included. For all lags, use
`lag0 = 1 - nb` and `nlags = na + nb - 1`: CMSIS-DSP's `arm_correlate_q15`
gives the same values in the same order. Every output is the exact sum
rounded once and saturated. `shift` divides by a further power of two, and
`shift = ceil(log2 min(na, nb))` rules out saturation. The sums stay 32-bit
(q31: 64-bit) whenever max |a| sum |b| or max |b| sum |a| allows it, which a
filter with sum |h| < 2 or 12-bit ADC data always does, and widen otherwise.

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_conv_q15`, 64 x 64 | 20270 | 123619 (`fast` 42252) | 0.5 LSB | 1 LSB |
| `fd_conv_q31`, 64 x 64 | 47073 | 49642 | 0.5 LSB | 1 LSB |
| `fd_corr_q15`, 256 x 256, all lags | 614396 | 1738429 | 0.5 LSB | 1 LSB |
| `fd_corr_q31`, 64 x 64, all lags | 46647 | 51009 | 0.5 LSB | |
| `fd_conv_f32`, 64 x 64 | 61126 | 500890 (soft float) | mean 0.42 ulp | mean 6.9 ulp |
| `fd_corr_f32`, 64 x 64, all lags | 62492 | 481099 (soft float) | mean 0.38 ulp | mean 6.2 ulp |
| `fd_corr_fft_q15`, 256 x 256 in 512 | 131431 | | 41 dB SNR | |
| `fd_autocorr_q15`, 256, 16 lags | 38605 | | 0.5 LSB | |
| `fd_peak_q15`, 128 | 1325 | | 0.5 LSB (Q16.16) | |
| `fd_levinson_q31`, order 10 | 4909 | 3779 | 7.7 LSB | 82 LSB |

- `fd_conv_f32`, `fd_conv_part_f32` and `fd_corr_f32` convert each input to
  q31 at one exponent and run the q31 kernels, with one rounding per output.
  You pass a q31 work buffer of `na + nb` values. Values more than about
  42 dB below the largest in their input lose bits, and on inputs like that
  the errors can be larger than soft float's. The table's errors are on
  white noise, and `docs/accuracy.md` has inputs with quiet halves.
- `fd_corr_fft_q15` correlates two zero-padded blocks through q15 FFTs, in
  place. At 256 x 256 it is 4.7 times faster than the direct sum, with the
  error at least 41 dB below the output's power (46 dB on average on
  full-scale noise) instead of exact. On delays with noise down to 0 dB SNR,
  and on chirps, it finds the same peak lag as the exact sum.
- `fd_autocorr_q15` / `_q31` return `r[k] 2^e` with `r[0]` in [2^30, 2^31):
  31 significant bits at any signal level, the input `fd_levinson_q31`
  wants.
- `fd_peak_q15` / `_q31` return the index of the largest value, moved to the
  vertex of the parabola through it and its neighbours, in Q16.16.
- `fd_levinson_q31` gives the predictor `x[n] ~ sum a[j - 1] x[n - j]`, with
  `a` in Q(31 - `ashift`), the reflection coefficients and the remaining
  error power. Each step's sums are exact and rounded once. Its error is
  against a `double` recursion on the same autocorrelation (AR(2) data,
  poles 0.3-0.7). CMSIS-DSP rounds every product and divides through a q15
  reciprocal.

## Adaptive filters (`fd_lms.h`)

An adaptive filter learns its coefficients as it runs: it cancels an echo,
removes hum picked up by a reference sensor, or identifies an unknown
system. These are LMS in q15 and q31, plain (`w += mu e x`), normalised
(`mu e x / x'x`) and sign-error (`mu sign(e) x`), here with 32 taps on
blocks of 64. The weights are Q30 in 32 bits for both formats, so small
updates still move them. The update for one sample runs in the same pass
over the taps as the next sample's output. Outputs are exact sums rounded
once and saturated, and the normalised step is an exact division, once per
sample.

| function | cycles | CMSIS-DSP |
|---|--:|--:|
| `fd_lms_q15` | 35585 | 42696 |
| `fd_lms_q15` normalised | 40057 | 48624 |
| `fd_lms_q15` sign-error | 35550 | |
| `fd_lms_q31` | 64066 | 71613 |
| `fd_lms_q31` normalised | 80848 | |

Identifying a 16-tap system with noise 60 dB down at mu = 2^-10, the weights
end within the error of a `double` LMS on the same data, 42 dB below the
system's norm after 60000 samples. CMSIS-DSP's q15 LMS truncates its
updates, drifts, and ends 0.5 dB *above* the norm
([docs/accuracy.md](../docs/accuracy.md)). `fd_lms_coefs_q15` / `_q31` read
the taps back. `fd_lms_reset_*` clears the history and keeps the weights,
and `fd_lms_set_mu_*` changes the step size between calls.

## Sample-rate conversion (`fd_resample.h`)

Audio at 32 kHz has to play at 48 kHz, and a fast ADC stream has to come
down to a few kHz. `fd_resample_q15` / `_q31` change the rate by l / m with
an FIR prototype at l times the input rate (gain l, as for
`fd_fir_interp`), split into l branches. Only the outputs that are kept are
computed, each its branch's exact sum rounded once: the same bits as
interpolating by l and keeping every m-th output. When a block holds at
least four outputs per branch they go through the decimator kernel four at a
time, and otherwise one by one (48 kHz from 44.1 kHz, 160 / 147). A block
can be any length, and the function returns how many outputs it wrote.
`fd_resample_design` builds the prototype as a windowed-sinc lowpass at a
fraction of the lower Nyquist frequency.

`fd_halfband_decim_*` / `fd_halfband_interp_*` change the rate by 2 with a
half-band lowpass of 4k - 1 taps, whose centre is 1/2 and whose other even
taps are zero. Only the 2k nonzero taps are stored and multiplied, and the
centre is an exact add: the interpolator's odd outputs are just delayed
inputs. The rounding is the same, and so are the bits, as the full-length
filter through `fd_fir_decim`. `fd_halfband_design` makes one with exactly
zero taps where they belong.

`fd_cic_decim_q15` / `_q31` average and decimate by `ratio` with `order` (1
to 6) integrators and combs, adds and subtracts only, which makes them the
cheapest way down by a large factor. The registers are sized from the input
width you give (`in_bits`): 32 bits when the result fits, which 12-bit ADC
data does up to order 3 by 101 or order 2 by 1024, and 64 otherwise. The sum
is exact and comes out as q31 with the bits the averaging adds kept, rounded
once when it is wider than 32 bits. `fd_cic_gain` gives the gain that is
left, ratio^order over a power of two, and `fd_cic_comp_design` the FIR
that flattens the passband after it. Each decimator is about 4 KB of code, a
loop per order and width.

| function | cycles | CMSIS-DSP | error | CMSIS-DSP error |
|---|--:|--:|--:|--:|
| `fd_resample_q15` 3 / 2, 24 taps, 64 in | 6577 | 18188 | 0.5 LSB | 1 LSB |
| `fd_resample_q31` 3 / 2, 24 taps, 64 in | 12347 | 19674 | 0.5 LSB | 1 LSB |
| `fd_resample_q15` 160 / 147, 1280 taps, 64 in | 7923 | | 0.5 LSB | |
| `fd_halfband_decim_q15`, 31 taps, 64 in | 2988 | | 0.5 LSB | |
| `fd_halfband_interp_q15`, 31 taps, 64 in | 5293 | | 0.5 LSB | |
| `fd_cic_decim_q15` order 3 by 64, 12-bit, 256 in | 2012 | | exact | |
| the same on 16-bit input (64-bit registers) | 2932 | | 0.5 LSB | |

- CMSIS-DSP has no rational resampler. Its rows interpolate by 3 with
  `arm_fir_interpolate_*` and keep every second output, computing three
  outputs for every two kept. Its q31 interpolator wraps past full scale.
- The half-band filters take half the time of `fd_fir_decim` /
  `fd_fir_interp` by 2 on the same 31 taps (6386 and 10384).
- 160 / 147 is 124 cycles per output with 8 taps per branch: 8% of the F103
  for 48 kHz out.
