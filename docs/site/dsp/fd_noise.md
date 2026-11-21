<p class="fl-header-name">fd_noise.h</p>

# Noise

Noise is a test signal, a dither, and a model of the real world.
`fd_noise.h` generates uniform, Gaussian and triangular noise in q15 and
q31, from flexutils' `xoshiro128**` generator, so a seed always gives the
same block.

## Gaussian noise at a set level

```{literalinclude} ../examples/fd_noise.c
:language: c
```

Which prints

```{literalinclude} ../examples/fd_noise.out
:language: text
```

Asked for a standard deviation of 3277 LSB, 0.1 of full scale, the 10000
samples measure 3258, 0.6% off, and a mean of 42 LSB. Both are what a block
of that size gives by chance.

## The three kinds

Uniform noise makes every value of the format equally likely. Gaussian
noise has zero mean and the standard deviation you ask for, saturated at
full scale. It uses the Box-Muller method, which keeps the tails out to 6.6
sigma, where a sum of uniforms would cut them off earlier. Triangular noise
spans $(-2^bits, 2^bits)$ LSB, with `bits` up to 15. Add it before dropping
bits from a signal, and the error of the dropped bits becomes plain noise
instead of distortion that follows the signal.

## How it was checked

- Gaussian, over 200000 samples of each format: the mean within 0.01 sigma
  of 0, the variance within 1%, the kurtosis within 0.06 of 3, and the share
  past 3 sigma within 0.0006 of 0.0027.
- Uniform q15: chi-square over all 65536 values from 4 10^6 samples, within
  6 standard deviations of its mean.
- Triangular: range, mean and variance at 4 widths.

## Numbers

Blocks of 64, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles |
|---|--:|
| `fd_noise_uniform_q15` | {cy}`fd_noise_uniform_q15, 64` |
| `fd_noise_gauss_q15` | {cy}`fd_noise_gauss_q15 (Box-Muller), 64` |
| `fd_noise_tpdf_q15` | {cy}`fd_noise_tpdf_q15, 64` |

A sum of 12 uniforms takes {cy}`gauss q15, sum of 12 uniforms` cycles for 64 Gaussian samples but cuts the
tails at 6 sigma.

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
