# flexdsp changelog

## Unreleased

- `fd_types.h`: complex sample types `fd_cq15`, `fd_cq31`, `fd_cf32`, and `FD_Status`.
- `flexdsp.mk`, and `dsp` in `FLEXLIB_LIBS`.
- `fd_vec.h`: q15 / q31 block arithmetic (add, sub, mul, scale, shift, offset, negate, abs, clip, fill, copy), conversions (q7, q15, q31, float), dB, statistics (sum, mean, dot, power, rms, var, std, min, max, absmax) and complex vectors (multiply, conjugate, magnitude, power, dot, interleave). Every result within 0.5 LSB except dB; statistics from exact sums.
- `fd_fir.h`: FIR filters, decimators and polyphase interpolators in q15 / q31 (exact sums rounded once, for any coefficients), windowed-sinc design (lowpass, highpass, bandpass, bandstop; five windows), quantisation and gain helpers. `FD_Window` in `fd_types.h`.
- `fd_biquad.h`: DF1 biquad cascades in q15 / q31 (each section's exact sum rounded once, any coefficients; optional first-order error feedback for q31), RBJ design (lowpass, highpass, bandpass, notch, peak, shelves, allpass) with low-frequency poles kept exact, Butterworth cascades, quantisation with a stability check.
- `fd_filt.h`: DC blocker, one-pole smoother, moving average (exact running sum), median of 3 and 5.
- `FD_Band` moved to `fd_types.h`.
