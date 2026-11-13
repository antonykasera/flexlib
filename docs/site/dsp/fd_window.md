<p class="fl-header-name">fd_window.h</p>

# Windows

Window functions in q15 and q31, generated at run time for any length.

Forms
: Periodic windows are the DFT's form, for spectra; symmetric ones are for
  filter design. Apply a window with {c:func}`fd_mul_q15` or
  {c:func}`fd_mul_q31`.

Fixed windows
: Rectangular, Hann, Hamming, Blackman, Blackman-Harris, Nuttall, flat-top
  and Bartlett, from flexmath's cosine on exact binary angles: q15 within
  0.5 LSB, q31 within 3 LSB of the exact value.

Parametric windows
: Kaiser (beta), Gaussian (sigma, relative to the half length) and Tukey
  (alpha, the tapered fraction) are computed in float: q15 within 0.52 LSB,
  q31 within 1.5 10^-6.

Gains
: {c:func}`fd_window_gains_q15` gives the coherent gain (sum w / n), which
  scales a tone's bin, and the power gain (sum w^2 / n), which scales noise.

Asserts
: n = 0, beta < 0, sigma <= 0, alpha outside [0, 1].

Checked
: - Every fixed window at every length up to 4096, symmetric and periodic,
    against `long double`: q15 at most 0.5 LSB, q31 at most 2.74 LSB.
  - Each parametric window at 3 parameters and 7 lengths from 1 to 1025,
    both forms, against `long double`.
  - The gains against exact sums over 1024 points of each fixed window.

## API

```{doxygenfunction} fd_window_q15
```

```{doxygenfunction} fd_window_q31
```

### Parametric windows

```{doxygenfunction} fd_window_kaiser_q15
```

```{doxygenfunction} fd_window_kaiser_q31
```

```{doxygenfunction} fd_window_gauss_q15
```

```{doxygenfunction} fd_window_gauss_q31
```

```{doxygenfunction} fd_window_tukey_q15
```

```{doxygenfunction} fd_window_tukey_q31
```

```{doxygenfunction} fd_window_gains_q15
```

```{doxygenfunction} fd_window_gains_q31
```

## Numbers

256 points, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles | max error, LSB |
|---|--:|--:|
| `fd_window_q15`, Hann | 23464 | 0.5 |
| `fd_window_q31`, Blackman-Harris | 54203 | 2.74 |
| `fd_window_kaiser_q15`, beta 8 | 750638 | 0.52 |

CMSIS-DSP's `arm_hanning_f32` takes 396495 cycles for the same 256 points on
the soft-float build.

## Example

```{literalinclude} ../examples/fd_window.c
:language: c
```
