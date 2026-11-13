<p class="fl-header-name">fd_fir.h</p>

# FIR filters

FIR filters, decimators and interpolators in q15 and q31, and windowed-sinc
design in float.

Rounding
: Every output is the exact sum rounded once (0.5 LSB) and saturated, for
  any coefficients.

Order
: `h[0]` multiplies the newest sample.

State
: The caller's buffer holds the `ntaps - 1` history samples plus a chunk of
  any size (`FD_FIR_STATE_LEN`). A block runs in chunk-sized pieces and the
  history moves once per chunk; the chunk changes the speed, not the
  results.

Blocks
: Any `n`. Decimators return the outputs written; interpolators write `n l`
  outputs and must not overlap their input.

Asserts
: `ntaps` or the factor 0, a state buffer shorter than `ntaps`.

Checked
: - Bit for bit against a 128-bit convolution at 16 lengths from 1 to 64
    taps, with $sum |h|$ below, at and far above 2, on random, alternating
    full-scale and constant -1 input; six chunk sizes, blocks cut at
    random, in place, and one sample at a time.
  - Decimators by 1 to 7 and interpolators by 1 to 8 against the same
    reference, decimated or zero-stuffed.
  - {c:func}`fd_fir_design` against a windowed sinc in double.

## API

```{doxygendefine} FD_FIR_STATE_LEN
```

```{doxygenstruct} FD_FirQ15
```

```{doxygenstruct} FD_FirQ31
```

```{doxygenfunction} fd_fir_init_q15
```

```{doxygenfunction} fd_fir_init_q31
```

```{doxygenfunction} fd_fir_reset_q15
```

```{doxygenfunction} fd_fir_reset_q31
```

```{doxygenfunction} fd_fir_q15
```

```{doxygenfunction} fd_fir_q31
```

```{doxygenfunction} fd_fir_sample_q15
```

```{doxygenfunction} fd_fir_sample_q31
```

### Decimators

```{doxygenstruct} FD_FirDecimQ15
```

```{doxygenstruct} FD_FirDecimQ31
```

```{doxygenfunction} fd_fir_decim_init_q15
```

```{doxygenfunction} fd_fir_decim_init_q31
```

```{doxygenfunction} fd_fir_decim_q15
```

```{doxygenfunction} fd_fir_decim_q31
```

### Interpolators

```{doxygendefine} FD_FIR_INTERP_STATE_LEN
```

```{doxygenstruct} FD_FirInterpQ15
```

```{doxygenstruct} FD_FirInterpQ31
```

```{doxygenfunction} fd_fir_interp_init_q15
```

```{doxygenfunction} fd_fir_interp_init_q31
```

```{doxygenfunction} fd_fir_interp_q15
```

```{doxygenfunction} fd_fir_interp_q31
```

### Design

```{doxygenfunction} fd_fir_design
```

```{doxygenfunction} fd_fir_quantize_q15
```

```{doxygenfunction} fd_fir_quantize_q31
```

```{doxygenfunction} fd_fir_gain_q15
```

```{doxygenfunction} fd_fir_gain_q31
```

## Numbers

32 taps, blocks of 64, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.
Errors over 10^6 random samples through a Hamming lowpass.

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_fir_q15` | 8078 | 19863 | 0.5 | 1 |
| `fd_fir_q15`, $sum \|h\| >= 2$ | 18134 | | 0.5 | |
| `fd_fir_q31` | 17836 | 16593 | 0.5 | 1 |
| `fd_fir_decim_q15` by 4 | 3686 | 4854 | 0.5 | |
| `fd_fir_interp_q15` by 4 | 12522 | 20806 | 0.5 | |
| `fd_fir_sample_q15` | 453 | | 0.5 | |

CMSIS-DSP: `arm_fir_q15`, `arm_fir_q31`, `arm_fir_decimate_q15`,
`arm_fir_interpolate_q15`. `arm_fir_fast_q15` takes 13941 cycles (1 LSB),
`arm_fir_fast_q31` 22446 (17 LSB).

## Example

```{literalinclude} ../examples/fd_fir.c
:language: c
```

## How it works

Init computes $sum |h|$ ({c:func}`fd_fir_gain_q15`). Below 2, every partial sum of a
q15 filter fits 32 bits and of a q31 filter 64 bits, and the narrow kernel
runs; at 2 or more the kernel keeps 64-bit (q31: 96-bit) sums, at about
twice the cycles. An interpolator decides per branch.
