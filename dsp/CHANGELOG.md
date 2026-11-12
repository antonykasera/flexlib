# flexdsp changelog

## 0.3.0 (2026-11-12)

- Licensed under Apache-2.0 (was MIT; earlier releases are granted under Apache-2.0 too).
- `fd_cfft_q31` / `fd_rfft_q31` and their inverses: faster stages without twiddles (5-6%). Results unchanged.
- `fd_irfft_q15` / `_q31`: one pass fewer (3-4% faster). Results change in the last bit, within the same bound.
- `fd_corr.h`: convolution (`fd_conv_q15` / `_q31`, and `fd_conv_part_*` for a range of outputs), correlation over any range of lags (`fd_corr_q15` / `_q31`), autocorrelation with a block exponent (`fd_autocorr_q15` / `_q31`), the peak of a block refined to a fraction of a sample (`fd_peak_q15` / `_q31`), Levinson-Durbin (`fd_levinson_q31`), and correlation through q15 FFTs (`fd_corr_fft_q15`). Every output of the direct functions is its exact sum rounded once.
- `fd_lms.h`: LMS adaptive filters in q15 / q31, plain, normalised and sign-error, with 32-bit weights and exact output sums.
- `fd_resample.h`: rational resampling by l / m over polyphase branches (`fd_resample_q15` / `_q31`, only the kept outputs computed, the same bits as interpolating by l and keeping every m-th), half-band decimators and interpolators by 2 (`fd_halfband_decim_*`, `fd_halfband_interp_*`, the zero taps skipped and the centre exact), and a CIC decimator (`fd_cic_decim_q15` / `_q31`, orders 1-6, exact in wrapping 32- or 64-bit registers, q31 out), with design helpers for each (`fd_resample_design`, `fd_halfband_design`, `fd_cic_comp_design`).

## 0.2.0 (2026-11-11)

- `fd_fft.h`: complex FFTs (`fd_cfft_q15` / `_q31`, any power of two from 2 to `FD_FFT_MAX`), real FFTs (`fd_rfft_q15` / `_q31`, packed n/2 bins in place) and their inverses. Radix-4 with block floating point: each stage scales by the block's own headroom and rounds its exact result once, so nothing saturates or wraps; the result's exponent is returned. `fd_fft_rescale_q15` / `_q31` convert to a given exponent (CMSIS-DSP's X / n among them), `fd_fft_db_q15` / `_q31` give power in dB with the exponent folded in.
- `FD_FFT_MAX` (default 1024): 2048 and 4096 add a 4 KB table; up to 1024 the twiddles come from flexmath's sine table.
- `fd_headroom_q15` / `_q31`: the spare sign bits of a block.

## 0.1.1 (2026-11-11)

- `fd_biquad_q15` / `_q31`: faster sections whose coefficients sum to 4 or more (shelves, peaks). Results unchanged.

## 0.1.0 (2026-11-11)

First release.

- `flexdsp.mk`, and `dsp` in `FLEXLIB_LIBS`.
- `fd_types.h`: complex sample types `fd_cq15`, `fd_cq31`, `fd_cf32`, `FD_Status`, `FD_Window`, `FD_Band`.
- `fd_vec.h`: q15 / q31 block arithmetic (add, sub, mul, scale, shift, offset, negate, abs, clip, fill, copy), conversions (q7, q15, q31, float), dB, statistics (sum, mean, dot, power, rms, var, std, min, max, absmax) and complex vectors (multiply, conjugate, magnitude, power, dot, interleave). Every result within 0.5 LSB except dB; statistics from exact sums.
- `fd_fir.h`: FIR filters, decimators and polyphase interpolators in q15 / q31 (exact sums rounded once, for any coefficients), windowed-sinc design (lowpass, highpass, bandpass, bandstop; eight windows), quantisation and gain helpers.
- `fd_biquad.h`: DF1 biquad cascades in q15 / q31 (each section's exact sum rounded once, any coefficients; optional first-order error feedback for q31), RBJ design (lowpass, highpass, bandpass, notch, peak, shelves, allpass) with low-frequency poles kept exact, Butterworth cascades, quantisation with a stability check.
- `fd_filt.h`: DC blocker, one-pole smoother, moving average (exact running sum), median of 3 and 5.
- `fd_window.h`: windows for any length, symmetric or periodic (rectangular, Hann, Hamming, Blackman, Blackman-Harris, Nuttall, flat-top, Bartlett; Kaiser, Gaussian, Tukey), and their coherent and power gains.
- `fd_tone.h`: Goertzel single-bin detection, NCO (sin / cos of a 32-bit phase, chirps, complex output), recursive oscillator.
- `fd_noise.h`: uniform, Gaussian (Box-Muller) and triangular (dither) noise.
