<p class="fl-header-name">fd_window.h</p>

# Windows

An FFT treats a block as if it repeated forever. Unless the block holds a
whole number of periods, the jump at its ends spreads a tone across many
bins. A window tapers the block towards zero at both ends to stop that.
Windows also shape the response of FIR designs. `fd_window.h` generates
them in q15 and q31 at run time, for any length, and gives the gains you
need to read levels off a windowed spectrum.

## A Hann window and its gains

```{literalinclude} ../examples/fd_window.c
:language: c
```

Which prints

```{literalinclude} ../examples/fd_window.out
:language: text
```

The periodic Hann window of 256 points starts at 0 and peaks at the middle,
where its value of 1 saturates to 32767. Its coherent gain is 0.5: a tone in
a Hann-windowed spectrum reads half, 6.02 dB, low. Its power gain is 0.375,
which is how much noise power the window lets through. To apply a window,
multiply it into the block with {c:func}`fd_mul_q15` or
{c:func}`fd_mul_q31`.

## Periodic or symmetric

Periodic windows are the form for spectra: a periodic window of n points is
the symmetric window of n + 1 points without its last point, so windows of
consecutive blocks join without a seam. Symmetric windows are the
form for filter design. Each function takes a flag that picks one.

## Which windows

Rectangular, Hann, Hamming, Blackman, Blackman-Harris, Nuttall, flat-top and
Bartlett are computed from flexmath's cosine on exact binary angles. q15 is
within 0.5 LSB and q31 within 3 LSB of the exact value. Kaiser (beta),
Gaussian (sigma, relative to the half length) and Tukey (alpha, the tapered
fraction) take a parameter and are computed in float: q15 within 0.52 LSB,
q31 within 1.5 10^-6.

{c:func}`fd_window_gains_q15` gives the coherent gain, $sum w / n$, and the
power gain, $sum w^2 / n$. An empty window, a negative beta, a sigma of 0 or
less and an alpha outside $[0, 1]$ stop at an assert.

## How it was checked

- Every fixed window at every length up to 4096, symmetric and periodic,
  against `long double`: q15 at most 0.5 LSB, q31 at most 2.74 LSB.
- Each parametric window at 3 parameters and 7 lengths from 1 to 1025, both
  forms, against `long double`.
- The gains against exact sums over 1024 points of each fixed window.

## Numbers

256 points, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles | max error, LSB |
|---|--:|--:|
| `fd_window_q15`, Hann | {cy}`fd_window_q15 Hann, 256` | 0.5 |
| `fd_window_q31`, Blackman-Harris | {cy}`fd_window_q31 Blackman-Harris, 256` | 2.74 |
| `fd_window_kaiser_q15`, beta 8 | {cy}`fd_window_kaiser_q15 beta 8, 256` | 0.52 |

CMSIS-DSP's `arm_hanning_f32` takes {cy}`arm_hanning_f32, 256` cycles for the same 256 points on
the soft-float build.

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
