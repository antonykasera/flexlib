<p class="fl-header-name">fd_corr.h</p>

# Correlation

Correlation tells you how well two signals match at each shift between them.
You use it to find the delay between two recordings of the same sound, or to
find a known pattern, such as a sync word or a chirp, inside a longer
capture. `fd_corr.h` computes it directly in q15 and q31, through FFTs for
long q15 blocks, and in float. It also has convolution, autocorrelation and
a Levinson-Durbin solver for linear prediction.

## Finding a delay

In this example `b` is a burst of noise, and `a` holds the same burst again,
delayed by 37.25 samples. The delay is not a whole number of samples, so
`a` is built by mixing the copies delayed by 37 and by 38:

```{literalinclude} ../examples/fd_corr.c
:language: c
```

Which prints

```{literalinclude} ../examples/fd_corr.out
:language: text
```

The correlation is largest at lag 37. {c:func}`fd_peak_q15` then fits a
parabola through that value and its two neighbours, and returns the
parabola's vertex: 37.11, close to the true 37.25. The parabola is an
estimate. How close it gets depends on the shape of the correlation peak,
not on the arithmetic.

## Lags and scaling

{c:func}`fd_corr_q15` computes one output for each lag, from `lag0` on:

$y[i] = sum a[j + lag0 + i] b[j]$

A term whose index falls outside either array counts as zero. That means a
lag can be negative, and it can run past the end of `a`. To get every lag
that has at least one term, start at $lag0 = 1 - nb$ and ask for
$na + nb - 1$ lags. This gives the same values in the same order as
CMSIS-DSP's `arm_correlate_q15`.

Each output is the exact sum of its products, rounded once. A sum of 256
products can be far larger than one q15 value, so you choose `shift`: the
sum is divided by $2^shift$ before it is rounded and saturated. The example
uses 8, because $2^8 = 256$ is the number of terms. In general
$shift = ceil(log2 min(na, nb))$ can never saturate. If you know your signals
stay well below full scale, a smaller shift keeps more of the result.

## Convolution

{c:func}`fd_conv_q15` computes $y[k] = sum a[i] b[k - i]$ for all
$na + nb - 1$ outputs. Think of it as a FIR filter with taps `b` run over the
whole block `a`, including the outputs where the filter only partly overlaps
the data at either end. If you only need some of those outputs,
{c:func}`fd_conv_part_q15` computes a range of them, with the same bits as
the full convolution. Convolution is correlation with `b` reversed, and the
two give identical results.

## Float

The float forms, {c:func}`fd_conv_f32`, {c:func}`fd_conv_part_f32` and
{c:func}`fd_corr_f32`, need no FPU. Each input is converted to q31 with one
exponent for the whole block, the q31 kernels compute exact sums, and each
result is rounded once to float. You pass a work buffer of $na + nb$ q31
values for the converted copies.

On signals of similar level this is more accurate than adding up floats one
product at a time. On white noise the mean error is
{acc}`correlate f32, 64 x 64; flexdsp; 4000 pairs of white noise @ mean` ulp,
against
{acc}`correlate f32, 64 x 64; CMSIS-DSP; 4000 pairs of white noise @ mean`
for `arm_correlate_f32`. The shared exponent is also the weakness. A value
more than about 42 dB below the loudest in its block keeps fewer bits than a
float would. With the second half of `a` 42 dB down, the mean error grows to
{acc}`correlate f32, 64 x 64; flexdsp; the same, a's second half 42 dB down @ mean`
ulp, against
{acc}`correlate f32, 64 x 64; CMSIS-DSP; the same, a's second half 42 dB down @ mean`
for CMSIS-DSP. If your blocks mix loud and very quiet passages, keep that in
mind, or correlate the quiet passage on its own.

The header gives the exact bound: within 0.5 ulp + $2^-31 S (M + W)$ of the
true result. S is the sum of |values| of the shorter input, M the largest
|value| of the longer one, and W the sum of the longer one's |values| that
the output uses. A NaN or inf anywhere in the inputs makes every output NaN.

## Long blocks: correlation through FFTs

A direct correlation of two 256-sample blocks over all lags takes
{cy}`fd_corr_q15, 256 x 256, all lags` cycles.
{c:func}`fd_corr_fft_q15` does the same work through q15 FFTs in
{cy}`fd_corr_fft_q15, 256 x 256 in 512`. The price is exactness: on average the
error sits
{acc}`correlate q15 through FFT, 256 x 256 in 512; flexdsp @ snr db` dB below
the output's power, and at least
{acc}`correlate q15 through FFT, 256 x 256 in 512; flexdsp @ worst pair dB` dB
below it in the worst case we measured. On delays in noise, down to 0 dB
SNR, and on chirps, it still finds the same peak lag as the direct sum.

The FFT correlation is circular. To get the ordinary correlation, zero-pad
both blocks to a power of two $n >= na + nb - 1$. Lag k then lands in slot k,
and lag -k in slot $n - k$. The function works in place and returns an
exponent: the result is $a[k] 2^e$.

## Autocorrelation and linear prediction

{c:func}`fd_autocorr_q15` correlates a block with itself and scales the result
so that $r[0]$ lies in $[2^30, 2^31)$. Whatever the signal level, r keeps
31 significant bits, and the function returns the exponent e with the true
value $r[k] 2^e$.

That is the input {c:func}`fd_levinson_q31` expects. It finds the order-p
predictor $x[n] ~ sum a[j - 1] x[n - j]$, the coefficients that best predict
each sample from the p before it. Each step sums exactly and rounds once.
Two things can go wrong, and both come back as a status. If r is not
positive definite, a reflection coefficient reaches 1 and you get
`FD_ERR_UNSTABLE`. If a coefficient does not fit the format you chose with
`ashift`, you get `FD_ERR_RANGE`.

## How it was checked

- Bit for bit against 128-bit sums: every size from 1 x 1 to 12 x 12 with
  four input kinds on each side (random full scale, small, alternating full
  scale, all most negative), nine larger pairs up to 300 x 300, and lags with
  no terms on either side.
- `make exhaustive`: every size to 16 against exact sums on 10^6 blocks, and
  the peak vertex for all 2^32 pairs of neighbour drops.
- {c:func}`fd_levinson_q31` bit for bit against a 128-bit model on 100000
  random autocorrelations at every ashift, statuses included, and its error
  on AR(2) data with poles from 0.3 to 0.99.
- {c:func}`fd_corr_fft_q15` over 600 trials of delays in noise and chirps:
  SNR above 40 dB and the peak lag of the exact sum every time.
- The float forms bit for bit against a model of the conversion and within
  the bound, worst 0.97 of it, on 1500 random shapes up to 80 x 80. Integer
  inputs come out correctly rounded, and convolution matches correlation with
  `b` reversed.

Arguments that are always a mistake stop at an assert: an empty input, a
shift past 16 (q31: 32), a range past the outputs, a peak search past 32768
values, an order outside 1 to 63, an ashift past 30. `y` must not overlap
either input, and the float forms' work buffer must not overlap anything.

## Numbers

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_conv_q15`, 64 x 64 | {cy}`fd_conv_q15, 64 x 64` | {cy}`arm_conv_q15, 64 x 64` | {acc}`conv q15, 64 x 64; flexdsp` | {acc}`conv q15, 64 x 64; CMSIS-DSP` |
| `fd_conv_q15`, 256 x 32 | {cy}`fd_conv_q15, 256 x 32` | {cy}`arm_conv_q15, 256 x 32` | 0.5 | 1 |
| `fd_conv_q15`, 64 x 64, both full scale | {cy}`fd_conv_q15, 64 x 64, both full scale (64-bit)` |  | 0.5 |  |
| `fd_conv_q31`, 64 x 64 | {cy}`fd_conv_q31, 64 x 64` | {cy}`arm_conv_q31, 64 x 64` | {acc}`conv q31, 64 x 64; flexdsp` | {acc}`conv q31, 64 x 64; CMSIS-DSP` |
| `fd_corr_q15`, 256 x 256, all lags | {cy}`fd_corr_q15, 256 x 256, all lags` | {cy}`arm_correlate_q15, 256 x 256` | 0.5 | 1 |
| `fd_corr_q15`, 256 x 256, lags 0 to 31 | {cy}`fd_corr_q15, 256 x 256, lags 0..31` |  | 0.5 |  |
| `fd_corr_q15`, the same on 12-bit data | {cy}`fd_corr_q15, 256 x 256, lags 0..31, 12-bit data` |  | 0.5 |  |
| `fd_corr_q31`, 64 x 64, all lags | {cy}`fd_corr_q31, 64 x 64, all lags` | {cy}`arm_correlate_q31, 64 x 64` | 0.5 |  |
| `fd_corr_fft_q15`, 256 x 256 in 512 | {cy}`fd_corr_fft_q15, 256 x 256 in 512` |  | {acc}`correlate q15 through FFT, 256 x 256 in 512; flexdsp @ worst pair dB` dB SNR |  |
| `fd_autocorr_q15`, 256, 16 lags | {cy}`fd_autocorr_q15, 256, 16 lags` |  | 0.5 |  |
| `fd_autocorr_q15`, the same on 12-bit data | {cy}`fd_autocorr_q15, 256, 16 lags, 12-bit data` |  | exact |  |
| `fd_autocorr_q31`, 128, 16 lags | {cy}`fd_autocorr_q31, 128, 16 lags` |  | 0.5 |  |
| `fd_peak_q15`, 128 | {cy}`fd_peak_q15, 128` |  | 0.5 (Q16.16) |  |
| `fd_levinson_q31`, order 10 | {cy}`fd_levinson_q31, order 10` | {cy}`arm_levinson_durbin_q31, order 10` | {acc}`Levinson q31, order 10; flexdsp` | {acc}`Levinson q31, order 10; CMSIS-DSP` |

`arm_conv_fast_q15` takes {cy}`arm_conv_fast_q15, 64 x 64` cycles for 64 x 64
({acc}`conv q15, 64 x 64 (fast); CMSIS-DSP` LSB). The Levinson errors are
against a `double` recursion on the same autocorrelation, over 2000 AR(2)
blocks of 1024 samples with poles from 0.3 to 0.7, in LSB of q31. CMSIS-DSP
rounds every product and divides through a q15 reciprocal.

Float, 64 x 64, soft float on the F103. Errors are in ulp of the exact
output, over 4000 pairs of white noise.

| function | cycles | CMSIS-DSP | mean error, ulp | CMSIS-DSP, ulp |
|---|--:|--:|--:|--:|
| `fd_conv_f32` | {cy}`fd_conv_f32, 64 x 64` | {cy}`arm_conv_f32, 64 x 64` | {acc}`conv f32, 64 x 64; flexdsp; 4000 pairs of white noise @ mean` | {acc}`conv f32, 64 x 64; CMSIS-DSP; 4000 pairs of white noise @ mean` |
| `fd_corr_f32`, all lags | {cy}`fd_corr_f32, 64 x 64, all lags` | {cy}`arm_correlate_f32, 64 x 64` | {acc}`correlate f32, 64 x 64; flexdsp; 4000 pairs of white noise @ mean` | {acc}`correlate f32, 64 x 64; CMSIS-DSP; 4000 pairs of white noise @ mean` |

## How it works

Runs of whole windows go through the FIR kernels, four outputs per pass
(q31: three), and so do the ends, with the samples past either end read as
zero. Most real inputs let the sums stay narrow: 32-bit in q15 and 64-bit in
q31, whenever $max |a| sum |b|$ or $max |b| sum |a|$ allows it. A filter
with $sum |h| < 2$ or 12-bit data always does. Otherwise the sums widen to
64 bits (q31: 96), at about twice the cycles, and the results are the same.

The float forms hold the longer input at its own exponent, every
$|q| < 2^31$, and the shorter one at the smallest exponent found with
$sum |q| < 2^32$. Each pair of indices meets in one output at most, so every
partial sum stays under $2^63$ and the 64-bit kernels stay exact.

## API

```{doxygenfunction} fd_corr_q15
```

```{doxygenfunction} fd_corr_q31
```

```{doxygenfunction} fd_conv_q15
```

```{doxygenfunction} fd_conv_q31
```

```{doxygenfunction} fd_conv_part_q15
```

```{doxygenfunction} fd_conv_part_q31
```

```{doxygenfunction} fd_conv_f32
```

```{doxygenfunction} fd_conv_part_f32
```

```{doxygenfunction} fd_corr_f32
```

```{doxygenfunction} fd_corr_fft_q15
```

```{doxygenfunction} fd_autocorr_q15
```

```{doxygenfunction} fd_autocorr_q31
```

```{doxygenfunction} fd_peak_q15
```

```{doxygenfunction} fd_peak_q31
```

```{doxygenfunction} fd_levinson_q31
```
