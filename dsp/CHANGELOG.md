# flexdsp changelog

## Unreleased

- `fd_types.h`: complex sample types `fd_cq15`, `fd_cq31`, `fd_cf32`, and `FD_Status`.
- `flexdsp.mk`, and `dsp` in `FLEXLIB_LIBS`.
- `fd_vec.h`: q15 / q31 block arithmetic (add, sub, mul, scale, shift, offset, negate, abs, clip, fill, copy), conversions (q7, q15, q31, float), dB, statistics (sum, mean, dot, power, rms, var, std, min, max, absmax) and complex vectors (multiply, conjugate, magnitude, power, dot, interleave). Every result within 0.5 LSB except dB; statistics from exact sums.
