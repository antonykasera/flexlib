# flexdsp

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
  stated.
- Programmer errors (a bad shift, `n == 0` where a result is needed) trip
  `FX_ASSERT`.
- On Cortex-M4 and M7 the DSP-extension instructions are used where they
  pay; results are bit-identical to the M3 build.

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
wrapping past 2^15.

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
