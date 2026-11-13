<p class="fl-header-name">fd_fft.h</p>

# FFT

Complex and real FFTs and their inverses in q15 and q31, in place, with
block floating point.

Sizes
: Complex: any power of two from 2 to `FD_FFT_MAX`. Real: 4 to
  `FD_FFT_MAX`, through a half-size complex transform and a split pass; the
  q15 buffer must be 4-byte aligned. `FD_FFT_MAX` is 1024 by default.

Scaling
: Each transform returns an exponent e: the result is $x[k] 2^e$ in integer
  units. Before each stage the block's headroom is measured and the stage
  scales by what its growth needs, left as well as right, so a quiet input
  keeps its bits. An all-zero block gives zeros and $e = 0$. The inverses
  include 1/n.

Rounding
: Every stage output is its exact value rounded once, to nearest with ties
  down. Nothing saturates or wraps, for any input.

Real layout
: n samples in, n/2 complex values out in the same buffer: $x[0] = Re X[0]$,
  $x[1] = Re X[n/2]$ (both bins are real), then $x[2k], x[2k + 1] = X[k]$
  for $k = 1$ to $n/2 - 1$.

Helpers
: {c:func}`fd_fft_rescale_q15` moves bins to another exponent, rounded and
  saturated: $to = log2 n$ gives $X / n$, CMSIS-DSP's scaling.
  {c:func}`fd_fft_db_q15` gives $10 log10 |X|^2$ with the exponent folded in,
  0 dB at full scale.

Asserts
: n not a power of two in range, a misaligned real q15 buffer.

Checked
: - Every size from 2 to 4096, 15 input kinds, every transform bit for bit
    against an integer model of each stage, which never overflows; stage
    inputs at each growth bound.
  - Against the double DFT at 16 to 1024 points on white noise, tones, two
    tones and -60 dB noise: within $2 + 0.6 log2 n$ LSB of the result's
    exponent, SNR over 55 dB (q15) and 150 dB (q31).
  - Inputs scaled by 2^k give the same bits with e larger by k; round
    trips; the real layout against the complex transform.
  - `make exhaustive`: every size and bin against the exact DFT on 10^6
    small blocks.

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

```{doxygenfunction} fd_fft_rescale_q15
```

```{doxygenfunction} fd_fft_rescale_q31
```

```{doxygenfunction} fd_fft_db_q15
```

```{doxygenfunction} fd_fft_db_q31
```

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

On white noise 40 dB below full scale, `fd_cfft_q15` at 256 points keeps
{acc}`cfft q15; flexdsp; 256; white, -40 dB @ snr db` dB (CMSIS-DSP {acc}`cfft q15; CMSIS-DSP; 256; white, -40 dB @ snr db`) and `fd_cfft_q31` at 1024 keeps {acc}`cfft q31; flexdsp; 1024; white, -40 dB @ snr db` dB ({acc}`cfft q31; CMSIS-DSP; 1024; white, -40 dB @ snr db`).
CMSIS-DSP scales by a fixed 1/4 per stage, which drops a quiet signal's low
bits. Its q31 butterflies keep the truncated high word of each product;
these sum each output exactly in 64 bits, which costs the cycles. The
rounding leaves a mean error of {acc}`cfft q15; flexdsp; 256 @ dc, real part` LSB at DC at 256 points
({acc}`cfft q15; flexdsp; 1024 @ dc, real part` at 1024); CMSIS-DSP's truncation, {acc}`cfft q15; CMSIS-DSP; 256 @ dc, real part` and
{acc}`cfft q15; CMSIS-DSP; 1024 @ dc, real part` in its format.

## Example

```{literalinclude} ../examples/fd_fft.c
:language: c
```

A full-scale sine reads $20 log10(n/2)$ dB, less the window's coherent
gain ({c:func}`fd_window_gains_q15`).
