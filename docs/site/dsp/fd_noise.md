<p class="fl-header-name">fd_noise.h</p>

# Noise

Uniform, Gaussian and triangular (dither) noise in q15 and q31, from the
`xoshiro128**` generator of flexutils (`fu_rand.h`).

Uniform
: Every value of the format equally likely.

Gaussian
: Zero mean, standard deviation sigma, saturated; Box-Muller, with tails to
  6.6 sigma.

Triangular
: In $(-2^bits, 2^bits)$ LSB, $bits <= 15$: dither for dropping bits.

Checked
: - Gaussian, over 200000 samples of each format: the mean within 0.01
    sigma of 0, the variance within 1% and the kurtosis within 0.06 of 3,
    the share past 3 sigma within 0.0006 of 0.0027.
  - Uniform q15: chi-square over all 65536 values from 4 10^6 samples,
    within 6 standard deviations of its mean.
  - Triangular: range, mean and variance at 4 widths.

## API

```{doxygenfunction} fd_noise_uniform_q15
```

```{doxygenfunction} fd_noise_uniform_q31
```

```{doxygenfunction} fd_noise_gauss_q15
```

```{doxygenfunction} fd_noise_gauss_q31
```

```{doxygenfunction} fd_noise_tpdf_q15
```

## Numbers

Blocks of 64, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles |
|---|--:|
| `fd_noise_uniform_q15` | {cy}`fd_noise_uniform_q15, 64` |
| `fd_noise_gauss_q15` | {cy}`fd_noise_gauss_q15 (Box-Muller), 64` |
| `fd_noise_tpdf_q15` | {cy}`fd_noise_tpdf_q15, 64` |

A sum of 12 uniforms takes {cy}`gauss q15, sum of 12 uniforms` cycles for 64 Gaussian samples but cuts the
tails at 6 sigma.

## Example

```{literalinclude} ../examples/fd_noise.c
:language: c
```
