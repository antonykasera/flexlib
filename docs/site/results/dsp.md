# flexdsp results

Every flexdsp case and its baselines, by module, in cycles.
[Results](index.md) gives the conditions.

## Vectors

[`fd_vec.h`](../dsp/fd_vec.md)

:::{bench-rows} fd_vec.h
arm_add_
arm_mult_
arm_scale_
arm_shift_
arm_offset_
arm_negate_
arm_abs_
arm_clip_
arm_fill_
arm_copy_
memcpy 128 B
arm_q31_to_q15
arm_q15_to_float
arm_float_to_q15
arm_mean_
arm_dot_prod_
arm_power_
arm_rms_
arm_var_
arm_std_
arm_max_
arm_absmax_
arm_cmplx_
:::

## FIR filters

[`fd_fir.h`](../dsp/fd_fir.md)

:::{bench-rows} fd_fir.h
arm_fir_q15
arm_fir_q31
arm_fir_fast_
arm_fir_f32
arm_fir_decimate_
arm_fir_interpolate_q15 by 4
:::

## Biquads

[`fd_biquad.h`](../dsp/fd_biquad.md)

:::{bench-rows} fd_biquad.h
arm_biquad_
:::

## Small filters

[`fd_filt.h`](../dsp/fd_filt.md)

:::{bench-rows} fd_filt.h
:::

## Windows

[`fd_window.h`](../dsp/fd_window.md)

:::{bench-rows} fd_window.h
arm_hanning_
:::

## Tones

[`fd_tone.h`](../dsp/fd_tone.md)

:::{bench-rows} fd_tone.h
:::

## Noise

[`fd_noise.h`](../dsp/fd_noise.md)

:::{bench-rows} fd_noise.h
:::

## FFT

[`fd_fft.h`](../dsp/fd_fft.md)

:::{bench-rows} fd_fft.h
arm_cfft_
arm_rfft_
:::

## Correlation

[`fd_corr.h`](../dsp/fd_corr.md)

:::{bench-rows} fd_corr.h
arm_conv_
arm_correlate_
arm_levinson_
:::

## LMS

[`fd_lms.h`](../dsp/fd_lms.md)

:::{bench-rows} fd_lms.h
arm_lms_
:::

## Block floating point

The conversions between float blocks and q31 at one exponent that the float
forms run on. They are internal to flexdsp.

:::{bench-rows}
fd_bfp_
:::

## Resampling

[`fd_resample.h`](../dsp/fd_resample.md)

:::{bench-rows} fd_resample.h
arm_fir_interpolate_q15 by 3
arm_fir_interpolate_q31 by 3
:::
