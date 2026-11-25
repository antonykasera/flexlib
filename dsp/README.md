# flexdsp

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexdsp-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

Signal processing for Cortex-M parts without an FPU. It provides block
arithmetic, FIR and biquad filters, windows, tone generation and detection,
noise, FFTs, correlation, adaptive filters, sample-rate conversion and speech
features in Q15 and Q31. The FIR, biquad, FFT, convolution, correlation and
sum functions also have float versions, implemented on the same integer
kernels. Written in C11 with no allocation.

## Example

A 31-tap lowpass at 0.1 cycles per sample, applied to DC plus a tone at the
Nyquist frequency:

```c
#include <stdio.h>

#include "fd_fir.h"

#define TAPS 31U

int main(void) {
  float hf[TAPS];
  fm_q15 h[TAPS], st[FD_FIR_STATE_LEN(TAPS, 64U)];
  if (fd_fir_design(hf, TAPS, FD_LOWPASS, 0.1f, 0.0f, FD_WIN_HAMMING) != FD_OK ||
      fd_fir_quantize_q15(hf, h, TAPS) != FD_OK) {
    return 1;
  }
  FD_FirQ15 f;
  fd_fir_init_q15(&f, h, TAPS, st, FD_FIR_STATE_LEN(TAPS, 64U));

  fm_q15 x[64], y[64];
  for (uint32_t i = 0U; i < 64U; i++) {
    x[i] = (i & 1U) != 0U ? 16384 : 0; /* DC 8192 plus a Nyquist tone */
  }
  fd_fir_q15(&f, x, y, 64U);
  printf("%d %d\n", y[62], y[63]);
  return 0;
}
```

Output:

```text
8196 8187
```

The residual tone is 4.5 LSB, about 65 dB below the input tone.

## Conventions

| Item | Convention |
| --- | --- |
| Sample types | `fm_q15`, `fm_q31`, `float` |
| Complex samples | Interleaved `{re, im}` (`fd_cq15`, `fd_cq31`, `fd_cf32`), as in CMSIS-DSP |
| Block functions | `(x, y, n)`, with `y` allowed to alias an input of the same type |
| Rounding | To nearest, ties up, saturated, within 0.5 LSB unless stated |
| Filters and correlation | Exact sum of products, rounded once, with no restriction on the coefficients |
| Argument errors | `FX_ASSERT` |
| Targets | The same C and the same results on all Cortex-M cores. The M4 and M7 builds do not use the DSP extension. |

Cycles are medians for a block of 64 on an STM32F103 at 72 MHz (`-O2`, warm
cache). Full results are in [docs/results/f103.md](../docs/results/f103.md),
and the error comparison with CMSIS-DSP in
[docs/accuracy.md](../docs/accuracy.md).

## Element-wise (`fd_vec.h`)

| Function | Cycles | CMSIS-DSP | Error | CMSIS-DSP error |
| --- | --: | --: | --: | --: |
| `fd_add_q15` / `fd_add_q31` | 516 / 595 | 539 / 1137 | exact | exact |
| `fd_mul_q15` / `fd_mul_q31` | 611 / 1331 | 667 / 843 | 0.5 LSB | 1 / 2 LSB |
| `fd_scale_q15` / `fd_scale_q31` | 590 / 1280 | 510 / 986 | 0.5 LSB | 1 LSB |
| `fd_shift_q15` (right) | 521 | 380 | 0.5 LSB | truncates |
| `fd_offset_q15`, `fd_negate_q15`, `fd_abs_q15` | 413, 410, 585 | 452, 562, 619 | exact | exact |
| `fd_clip_q15` | 576 | 692 | exact | exact |
| `fd_fill_q15`, `fd_copy_q15` | 144, 214 | 138, 231 | | `memcpy` 918 |
| `fd_add_f32` / `fd_mul_f32` / `fd_abs_f32` | 4121 / 2861 / 410 | 4120 / 2860 / 404 | one rounding / exact | the same |

## Conversions

| Function | Cycles | CMSIS-DSP | Error | CMSIS-DSP error |
| --- | --: | --: | --: | --: |
| `fd_q31_to_q15` | 431 | 365 | 0.5 LSB | 1 LSB |
| `fd_q15_to_f32` / `fd_f32_to_q15` | 1401 / 2359 | 4051 / 3283 | exact / 0.5 LSB | |
| `fd_db_q15` / `fd_db_q31` | 3963 | | 0.54 / 0.97 LSB | |

## Statistics

| Function | Cycles | CMSIS-DSP | Error | CMSIS-DSP error |
| --- | --: | --: | --: | --: |
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

Sums are exact and rounded once. The q31 dot product and power return Q16.48,
the CMSIS-DSP format, and saturate past 2^15. `fd_headroom_q15` / `_q31` return
the spare sign bits of a block. The float errors are for 64 values from 2^-10
to 2^10.

## Complex vectors

| Function | Cycles | CMSIS-DSP | Error | CMSIS-DSP error |
| --- | --: | --: | --: | --: |
| `fd_cmul_q15` / `fd_cmul_q31` (32) | 1712 / 2150 | 1300 / 1167 | 0.5 LSB | 2 LSB (Q3.13) |
| `fd_cmag_q15` / `fd_cmag_q31` | 3331 / 7384 | 8612 / 8848 | 0.5 LSB | 1 LSB (Q2.14) |
| `fd_cmag2_q15` | 718 | 958 | exact (UQ2.30) | Q3.13 |
| `fd_cdot_q15` | 1806 | | exact | |

`fd_cmag` returns UQ1.15 / UQ1.31, which holds magnitudes up to sqrt 2.

## FIR filters (`fd_fir.h`)

| Function (32 taps) | Cycles | CMSIS-DSP | Error | CMSIS-DSP error |
| --- | --: | --: | --: | --: |
| `fd_fir_q15` | 8078 | 19863 (`fast` 13941) | 0.5 LSB | 1 LSB |
| `fd_fir_q15`, sum \|h\| >= 2 | 18134 | | 0.5 LSB | |
| `fd_fir_q31` | 17836 | 16593 (`fast` 22446) | 0.5 LSB | 1 LSB (`fast` 17) |
| `fd_fir_decim_q15` by 4 | 3686 | 4854 | 0.5 LSB | |
| `fd_fir_interp_q15` by 4 | 12522 | 20806 | 0.5 LSB | |
| `fd_fir_f32` | 22735 | 180335 (soft float) | mean 0.37 ulp | mean 4.4 ulp |

Filters with sum |h| < 2 use 32-bit (q15) or 64-bit (q31) accumulators, and
wider ones use wider sums with the same results. `h[0]` multiplies the newest
sample. The state buffer holds `FD_FIR_STATE_LEN` samples, the history plus a
chunk. Larger chunks run faster and give the same results. Decimators accept
arbitrary `n` and return the number of outputs written. `fd_fir_design`
builds windowed-sinc lowpass, highpass, bandpass and bandstop filters, and
`fd_fir_quantize_q15` / `_q31` convert them.

`fd_fir_f32` runs the q31 kernel on block floating point with one rounding per
output. After the input level drops by more than 42 dB, about one chunk of
outputs carries fewer bits than float.

## Biquads (`fd_biquad.h`)

| Function (4 sections) | Cycles | CMSIS-DSP | Error, fc 0.01 | CMSIS-DSP error |
| --- | --: | --: | --: | --: |
| `fd_biquad_q15` | 6120 | 13291 (`fast` 8778) | 96 LSB | 355 LSB |
| `fd_biquad_q31` | 14200 | 11883 (`fast` 16631) | 80 LSB | 349 LSB |
| `fd_biquad_q31` with error feedback | 18052 | `32x64` 25739 | 5 LSB | `32x64` 1.7 LSB |
| `fd_biquad_f32` | 57635 | `df2T` 113574 | mean 207 ulp | mean 574 ulp |

DF1 sections with the CMSIS-DSP coefficient order and signs, in Q2.30 / Q2.14.
Section outputs are exact sums rounded once. Gain is applied as a shift on the
`b` coefficients. The errors are those of the recursion against the same
coefficients in double, over 10^6 samples of noise.

Sections with `|b0| + |b1| + |b2| + |a1| + |a2|` >= 4, typical of shelves and
peaks, sum the b and a parts separately with an overflow check. Sections where
the b or a part alone reaches 4 use 96 / 64-bit sums.

| 48 kHz EQ, 4 sections x 64 | q15 | q31 | q31 with error feedback |
| --- | --: | --: | --: |
| `bshift` 0 | 8070 | 16815 | 17831 |
| `bshift` 1 (+12 dB high shelf) | 9908 | 22818 | |

`fd_biquad_design` returns RBJ sections (lowpass, highpass, bandpass, notch,
peak, shelves, allpass) and keeps low-frequency poles exact in Q2.30.
`fd_biquad_butter` returns even-order Butterworth cascades.
`fd_biquad_quantize_q31` / `_q15` convert sections, select the `b` shift and
report sections made unstable by rounding.

## Small filters (`fd_filt.h`)

| Function | Cycles (64 samples) | Notes |
| --- | --: | --- |
| `fd_dcblock_q15` / `_q31` | 1064 / 2027 | `y = x - x1 + a y1` |
| `fd_onepole_q15` / `_q31` | 796 / 1404 | `y += a (x - y)`, with `fd_onepole_coef` for a -3 dB corner |
| `fd_movavg_q15`, n = 32 / 30 | 1264 / 2429 | Exact running sum, shift for powers of two |
| `fd_movavg_q31`, n = 32 | 2309 | |
| `fd_median5` | 30 | Branchless, also `fd_median3` |

## Windows (`fd_window.h`)

| Function (256 points) | Cycles | CMSIS-DSP | Error |
| --- | --: | --: | --: |
| `fd_window_q15` Hann | 23464 | `arm_hanning_f32` 396495 | 0.5 LSB |
| `fd_window_q31` Blackman-Harris | 54203 | | 3 LSB |
| `fd_window_kaiser_q15`, beta 8 | 750638 | | 0.52 LSB |

Rectangular, Hann, Hamming, Blackman, Blackman-Harris, Nuttall, flat-top and
Bartlett windows are generated at run time, symmetric or periodic, and checked
at all lengths up to 4096 (q31 within 2.74 LSB). Kaiser, Gaussian and Tukey are
computed in float (q31 within 1.5e-6). `fd_window_gains_q15` / `_q31` return
the coherent and power gains.

## Tones and noise (`fd_tone.h`, `fd_noise.h`)

| Function | Cycles (64 samples) | Notes |
| --- | --: | --- |
| `fd_goertzel_q15` / `_q31` | 1374 / 1921 | One DFT term, with `fd_goertzel_power` exact from the state |
| `fd_nco_q15` sin | 5303 | 0.5 LSB, 32-bit phase |
| `fd_nco_q31` sin + cos | 7067 | 1.11 LSB, linear chirps, complex output |
| `fd_osc_q15` sin / `fd_osc_q31` sin + cos | 2694 / 3925 | Amplitude within 7e-8, phase drift 5.5e-5 rad over 10^6 samples |
| `fd_noise_uniform_q15` | 631 | |
| `fd_noise_gauss_q15` | 14112 | Box-Muller, tails to 6.6 sigma |
| `fd_noise_tpdf_q15` | 1203 | Triangular dither of +-2^bits LSB |

## Transforms (`fd_fft.h`)

| Transform | Cycles | CMSIS-DSP | SNR, white noise | CMSIS-DSP SNR |
| --- | --: | --: | --: | --: |
| `fd_cfft_q15`, 256 | 32433 | 32665 | 72.4 dB | 60.1 dB |
| `fd_cfft_q15`, 1024 | 157075 | 159716 | 68.3 dB | 54.0 dB |
| `fd_cfft_q31`, 256 | 59677 | 36223 | 167.8 dB | 152.6 dB |
| `fd_rfft_q15`, 512 | 40107 | 109567 | 69.8 dB | 57.1 dB |
| `fd_rfft_q31`, 512 | 81801 | | 166.7 dB | 150.8 dB |
| `fd_icfft_q15`, 256 / `fd_irfft_q15`, 512 | 33849 / 39724 | | | |
| `fd_fft_db_q15`, 128 bins | 15160 | | 0.5003 LSB | |
| `fd_cfft_f32`, 256 | 85313 | 418737 | 151.8 dB | 140.5 dB |
| `fd_rfft_f32`, 512 | 107463 | 604927 | 151.7 dB | 139.4 dB |

Radix-4 decimation in time, in place. Complex transforms take power-of-two
lengths from 2 to `FD_FFT_MAX`, real transforms 4 to `FD_FFT_MAX`.
`FD_FFT_MAX` is 1024 by default and uses the flexmath sine table. 2048 and
4096 add a 4 KB table.

The transforms use block floating point. Each stage is scaled by the headroom
of the block, up or down, and its outputs are exact values rounded to nearest
with ties down. The transform returns an exponent e, and the result is
`x[k] 2^e`. No input saturates or wraps. White noise at -40 dB keeps 70.4 dB
SNR at 256 points (CMSIS-DSP 20.1). The q31 butterflies use exact 64-bit sums,
which accounts for the cycle difference against CMSIS-DSP.

| Item | Format |
| --- | --- |
| Complex | `fd_cq15` / `fd_cq31`, bin k at `x[k]` |
| Real | n samples in, n/2 complex values out in place. `x[0]` = Re X[0], `x[1]` = Re X[n/2], then X[k] for k = 1 .. n/2 - 1. 4-byte aligned. |
| Scaling | `fd_fft_rescale_q15(x, n, e, to)` moves bins to exponent `to`. `to = log2 n` gives the CMSIS-DSP scaling X / n. |
| dB | `fd_fft_db_q15` / `_q31` return 10 log10 \|X\|^2 with the exponent applied, 0 dB at full scale |
| Float | `fd_cfft_f32` / `fd_rfft_f32` and the inverses take a q31 work buffer of n values |

```c
static fm_q15 w[512], x[512] __attribute__((aligned(4)));
fm_q16 db[256];

fd_window_q15(w, 512U, FD_WIN_HANN, true);        /* once */
fd_mul_q15(x, w, x, 512U);
int32_t e = fd_rfft_q15(x, 512U);
fd_fft_db_q15((fd_cq15 *)(void *)x, e, db, 256U); /* db[0] mixes DC and n/2 */
```

A full-scale sine reads 20 log10(n/2) dB less the coherent gain of the window.

## Convolution and correlation (`fd_corr.h`)

| Function | Cycles | CMSIS-DSP | Error | CMSIS-DSP error |
| --- | --: | --: | --: | --: |
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

`fd_conv_q15` / `_q31` return all `na + nb - 1` outputs, `fd_conv_part_*` a
range, and `fd_corr_q15` / `_q31` a range of lags including negative ones.
`lag0 = 1 - nb` with `nlags = na + nb - 1` matches the output order of
`arm_correlate_q15`. `shift` divides by a further power of two, and
`shift = ceil(log2 min(na, nb))` prevents saturation.

| Function | Notes |
| --- | --- |
| `fd_conv_f32`, `fd_conv_part_f32`, `fd_corr_f32` | q31 kernels on block floating point, with a q31 work buffer of `na + nb` values. Values more than 42 dB below the input peak lose precision. |
| `fd_corr_fft_q15` | Correlation through q15 FFTs, in place. Error at least 41 dB below the output power. |
| `fd_autocorr_q15` / `_q31` | Return `r[k] 2^e` with `r[0]` in [2^30, 2^31), the input format of `fd_levinson_q31` |
| `fd_peak_q15` / `_q31` | Index of the maximum, refined by parabolic interpolation, in Q16.16 |
| `fd_levinson_q31` | Predictor coefficients in Q(31 - `ashift`), reflection coefficients and prediction error. Error against a `double` recursion on AR(2) data. |

## Adaptive filters (`fd_lms.h`)

| Function (32 taps) | Cycles | CMSIS-DSP |
| --- | --: | --: |
| `fd_lms_q15` | 35585 | 42696 |
| `fd_lms_q15` normalised | 40057 | 48624 |
| `fd_lms_q15` sign-error | 35550 | |
| `fd_lms_q31` | 64066 | 71613 |
| `fd_lms_q31` normalised | 80848 | |

LMS in q15 and q31, in plain (`w += mu e x`), normalised (`mu e x / x'x`) and
sign-error (`mu sign(e) x`) forms. Weights are Q30 in 32 bits for both formats.
Outputs are exact sums rounded once and saturated, and the normalised step is
an exact division per sample. Identifying a 16-tap system at mu = 2^-10, the
weights converge to within the error of a `double` LMS on the same data.
`fd_lms_coefs_q15` / `_q31` read the weights, `fd_lms_reset_*` clears the
history and keeps the weights, and `fd_lms_set_mu_*` changes the step size.

## Sample-rate conversion (`fd_resample.h`)

| Function (64 inputs) | Cycles | CMSIS-DSP | Error | CMSIS-DSP error |
| --- | --: | --: | --: | --: |
| `fd_resample_q15` 3 / 2, 24 taps | 6577 | 18188 \* | 0.5 LSB | 1 LSB |
| `fd_resample_q31` 3 / 2, 24 taps | 12347 | 19674 \* | 0.5 LSB | 1 LSB |
| `fd_resample_q15` 160 / 147, 1280 taps | 7923 | | 0.5 LSB | |
| `fd_halfband_decim_q15`, 31 taps | 2988 | | 0.5 LSB | |
| `fd_halfband_interp_q15`, 31 taps | 5293 | | 0.5 LSB | |
| `fd_cic_decim_q15` order 3 by 64, 12-bit, 256 inputs | 2012 | | exact | |
| `fd_cic_decim_q15`, 16-bit input (64-bit registers) | 2932 | | 0.5 LSB | |

\* `arm_fir_interpolate_*` by 3, keeping one output in two.

`fd_resample_q15` / `_q31` change the rate by l / m with a polyphase FIR
prototype at l times the input rate. The result equals interpolation by l
followed by decimation by m. Blocks can have arbitrary length, and the
function returns the number of outputs written. `fd_resample_design` builds
the prototype.

`fd_halfband_decim_*` / `fd_halfband_interp_*` change the rate by 2 with a
half-band filter of 4k - 1 taps, skipping the zero taps. The result equals the
full-length filter through `fd_fir_decim`. `fd_halfband_design` builds the
filter.

`fd_cic_decim_q15` / `_q31` decimate by `ratio` with `order` (1 to 6)
integrator and comb stages. Registers are 32 or 64 bits, selected from
`in_bits`. The output is q31. `fd_cic_gain` returns the residual gain and
`fd_cic_comp_design` a compensation FIR. The decimators take about 4 KB of
code.

## Speech features (`fd_mfcc.h`)

| Function (512 samples, 40 bands, 10 coefficients) | Cycles | CMSIS-DSP |
| --- | --: | --: |
| `fd_mel_q15` | 4774 | |
| `fd_log_q16` | 4651 | |
| `fd_dct2_q31`, 40 to 10 | 6235 | |
| Frame: Hann window, `fd_rfft_q15`, mel, log, DCT | 60247 | `arm_mfcc_q15` 199630 |

`fd_mel_q15` sums the power spectrum into mel bands, `fd_log_q16` returns
their natural log in Q16.16, and `fd_dct2_q31` returns the first DCT-II
coefficients. The pipeline, including the filter design, is integer-only and
gives the same results on a host and on the target. Against `double`, the
log-mel bands are within 0.031 and the coefficients within 0.012.

## Configuration

| Macro | Effect |
| --- | --- |
| `FD_FFT_MAX` | Largest FFT length, 1024 by default. 2048 and 4096 add a 4 KB table. |

## Usage

Select the library with `FLEXLIB_LIBS := dsp`, or link `flexlib::dsp` in
CMake. flexmath and flexutils are added as dependencies. Build integration is
described in the [root README](../README.md#usage).

## Testing

See [docs/testing.md](../docs/testing.md).
