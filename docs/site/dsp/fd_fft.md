<p class="fl-header-name">fd_fft.h</p>

# FFT

An FFT turns a block of samples into its spectrum: which frequencies are in
it, and how strong each one is. `fd_fft.h` has complex and real FFTs and
their inverses in q15, q31 and float, in place. The fixed-point transforms
use block floating point, so a quiet signal keeps its precision instead of
losing bits to fixed scaling.

## The spectrum of a sine

This takes a full-scale sine that falls exactly in bin 32 of a 512-point
transform, applies a Hann window and reads the spectrum in dB:

```{literalinclude} ../examples/fd_fft.c
:language: c
```

Which prints

```{literalinclude} ../examples/fd_fft.out
:language: text
```

The peak is in bin 32, at 42.14 dB. A full-scale sine reads
$20 log10(n / 2)$ dB, 48.16 dB at 512 points, less the window's coherent
gain of 6.02 dB ({c:func}`fd_window_gains_q15`). {c:func}`fd_fft_db_q15`
folds the transform's exponent into the result, so 0 dB is full scale
whatever scaling the FFT chose.

## Scaling and the exponent

The values in an FFT grow as they pass through its stages, and a
fixed-point FFT has to keep them from overflowing.
CMSIS-DSP scales down by a fixed amount at every stage, which drops a quiet
signal's low bits. These transforms measure the block's headroom before
each stage and scale by what that stage needs, up as well as down. Each
returns an exponent e: the result is $x[k] 2^e$. An all-zero block gives
zeros and $e = 0$. The inverses include the 1/n.

To get CMSIS-DSP's scaling, $X / n$, call {c:func}`fd_fft_rescale_q15` with
$to = log2 n$. It moves the bins to another exponent, rounded and
saturated.

## Sizes and layout

Complex transforms take any power of two from 2 to `FD_FFT_MAX`, 1024 by
default. Real transforms take 4 to `FD_FFT_MAX`, and the q15 buffer must be
4-byte aligned. A real transform turns n samples into n/2 complex values in
the same buffer. $x[0]$ holds $Re X[0]$ and $x[1]$ holds $Re X[n/2]$, both
of which are real, and $x[2k]$, $x[2k + 1]$ hold $X[k]$ for $k = 1$ to
$n/2 - 1$. A size out of range and a misaligned real q15 buffer stop at an
assert.

## Rounding

Every stage output is its exact value rounded once, to nearest with ties
down. Nothing saturates or wraps, for any input. The q31 transforms sum each
output exactly in 64 bits. That costs cycles against CMSIS-DSP, whose
butterflies keep the truncated high word of each product, and it gives more
SNR, as the table below shows.

## Float

{c:func}`fd_cfft_f32` and the other float forms convert the block to q31
with one exponent, run the q31 transform, and convert back. You pass a q31
work buffer of n values, which must not overlap x. The results are unscaled,
like the q31 forms, and the inverses include 1/n. In soft float they are
about five times faster than CMSIS-DSP's, with about 12 dB more SNR. A NaN
or inf in x makes every output NaN.

## How it was checked

- Every size from 2 to 4096, 15 input kinds, every transform bit for bit
  against an integer model of each stage, which never overflows, with stage
  inputs at each growth bound.
- Against the double DFT at 16 to 1024 points on white noise, tones, two
  tones and -60 dB noise: within $2 + 0.6 log2 n$ LSB of the result's
  exponent, and SNR over 55 dB (q15) and 150 dB (q31).
- Inputs scaled by 2^k give the same bits with e larger by k. Round trips,
  and the real layout against the complex transform.
- `make exhaustive`: every size and bin against the exact DFT on 10^6 small
  blocks.
- The float forms against the double DFT at 2 to 1024 points, inputs from
  2^-130 to 2^120: SNR over 130 dB. Zeros, NaN, and an output past the float
  range.

## Numbers

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache. SNR against the exact
DFT, full-scale white noise, 40 blocks.

| transform | cycles | CMSIS-DSP | SNR, dB | CMSIS-DSP, dB |
|---|--:|--:|--:|--:|
| `fd_cfft_q15`, 256 | {cy}`fd_cfft_q15, 256` | {cy}`arm_cfft_q15, 256` | {acc}`cfft q15; flexdsp; 256; white @ snr db` | {acc}`cfft q15; CMSIS-DSP; 256; white @ snr db` |
| `fd_cfft_q15`, 1024 | {cy}`fd_cfft_q15, 1024` | {cy}`arm_cfft_q15, 1024` | {acc}`cfft q15; flexdsp; 1024; white @ snr db` | {acc}`cfft q15; CMSIS-DSP; 1024; white @ snr db` |
| `fd_cfft_q31`, 256 | {cy}`fd_cfft_q31, 256` | {cy}`arm_cfft_q31, 256` | {acc}`cfft q31; flexdsp; 256; white @ snr db` | {acc}`cfft q31; CMSIS-DSP; 256; white @ snr db` |
| `fd_cfft_q31`, 1024 | {cy}`fd_cfft_q31, 1024` | {cy}`arm_cfft_q31, 1024` | {acc}`cfft q31; flexdsp; 1024; white @ snr db` | {acc}`cfft q31; CMSIS-DSP; 1024; white @ snr db` |
| `fd_rfft_q15`, 512 | {cy}`fd_rfft_q15, 512` | {cy}`arm_rfft_q15, 512` | {acc}`rfft q15; flexdsp; 512; white @ snr db` | {acc}`rfft q15; CMSIS-DSP; 512; white @ snr db` |
| `fd_rfft_q31`, 512 | {cy}`fd_rfft_q31, 512` |  | {acc}`rfft q31; flexdsp; 512; white @ snr db` | {acc}`rfft q31; CMSIS-DSP; 512; white @ snr db` |
| `fd_icfft_q15`, 256 | {cy}`fd_icfft_q15, 256` |  |  |  |
| `fd_irfft_q15`, 512 | {cy}`fd_irfft_q15, 512` |  |  |  |
| `fd_fft_db_q15`, 128 bins | {cy}`fd_fft_db_q15, 128 bins` |  |  |  |
| `fd_cfft_f32`, 256 | {cy}`fd_cfft_f32, 256` | {cy}`arm_cfft_f32, 256` | {acc}`cfft f32; flexdsp; 256; white @ snr db` | {acc}`cfft f32; CMSIS-DSP; 256; white @ snr db` |
| `fd_rfft_f32`, 512 | {cy}`fd_rfft_f32, 512` | {cy}`arm_rfft_fast_f32, 512` | {acc}`rfft f32; flexdsp; 512; white @ snr db` | {acc}`rfft f32; CMSIS-DSP; 512; white @ snr db` |
| `fd_icfft_f32`, 256 | {cy}`fd_icfft_f32, 256` |  |  |  |
| `fd_irfft_f32`, 512 | {cy}`fd_irfft_f32, 512` |  |  |  |

On white noise 40 dB below full scale, `fd_cfft_q15` at 256 points keeps
{acc}`cfft q15; flexdsp; 256; white, -40 dB @ snr db` dB (CMSIS-DSP {acc}`cfft q15; CMSIS-DSP; 256; white, -40 dB @ snr db`) and `fd_cfft_q31` at 1024 keeps {acc}`cfft q31; flexdsp; 1024; white, -40 dB @ snr db` dB ({acc}`cfft q31; CMSIS-DSP; 1024; white, -40 dB @ snr db`).
The f32 rows are soft float on the F103, where `arm_cfft_f32` and
`arm_rfft_fast_f32` call libgcc for each operation. CMSIS-DSP scales by a
fixed 1/4 per stage, which drops a quiet signal's low
bits. Its q31 butterflies keep the truncated high word of each product;
these sum each output exactly in 64 bits, which costs the cycles. The
rounding leaves a mean error of {acc}`cfft q15; flexdsp; 256 @ dc, real part` LSB at DC at 256 points
({acc}`cfft q15; flexdsp; 1024 @ dc, real part` at 1024); CMSIS-DSP's truncation, {acc}`cfft q15; CMSIS-DSP; 256 @ dc, real part` and
{acc}`cfft q15; CMSIS-DSP; 1024 @ dc, real part` in its format.

## API

```{doxygendefine} FD_FFT_MAX
```

```{doxygenfunction} fd_cfft_q15
```

```{doxygenfunction} fd_cfft_q31
```

```{doxygenfunction} fd_icfft_q15
```

```{doxygenfunction} fd_icfft_q31
```

```{doxygenfunction} fd_rfft_q15
```

```{doxygenfunction} fd_rfft_q31
```

```{doxygenfunction} fd_irfft_q15
```

```{doxygenfunction} fd_irfft_q31
```

### Float

```{doxygenfunction} fd_cfft_f32
```

```{doxygenfunction} fd_icfft_f32
```

```{doxygenfunction} fd_rfft_f32
```

```{doxygenfunction} fd_irfft_f32
```

### Helpers

```{doxygenfunction} fd_fft_rescale_q15
```

```{doxygenfunction} fd_fft_rescale_q31
```

```{doxygenfunction} fd_fft_db_q15
```

```{doxygenfunction} fd_fft_db_q31
```
