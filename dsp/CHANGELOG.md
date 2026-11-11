# flexdsp changelog

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
