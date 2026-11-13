# Block floating point

The FFT functions in [fd_fft](../dsp/fd_fft.md) use block floating point. All
values in the block share one exponent. Each transform scales its stages to
fit the data and returns the exponent.

This page describes the result format, how each stage is scaled, how the
stages round, and how to use the exponent.

## Terms

Block floating point
: A block of fixed-point values with one shared exponent e. Each value v
  represents $v 2^e$.

Headroom
: The number of unused sign bits in a block. A q15 block whose largest
  magnitude is 1000 needs 10 bits plus sign, and has 5 bits of headroom.
  {c:func}`fd_headroom_q15` returns it.

Stage
: One pass over the block. A radix-2 stage combines pairs of values. A
  radix-4 stage combines groups of four.

Twiddle factor
: The complex constant $exp(-2 pi i k / n)$ that a stage multiplies some of
  its inputs by. The first radix-4 stage has none.

## Result format

Each transform works in place and returns e:

```c
int32_t e = fd_cfft_q15(x, 256);
/* bin k is (x[k].re + i x[k].im) * 2^e, in units of the input's LSB */
```

The result is the unscaled DFT sum. No factor $1 / n$ is applied. The
inverse transforms include $1 / n$. A block of zeros returns zeros and
$e = 0$.

## Stage scaling

Before each stage, a scan finds b, the number of bits that the largest
magnitude in the block needs. A stage can grow its largest magnitude by a
known factor. The stage shifts its input by enough bits to keep its output
inside the integer type.

A positive shift is to the right. A negative shift is to the left. A loud
block shifts right and a quiet block shifts left. The exponent e is the sum of
the shifts over all stages.

| stage | growth bound | q15 shift | q31 shift |
|---|---|---|---|
| radix-2 | 2 | $b - 14$ | $b - 30$ |
| radix-4, no twiddles | 4 | $b - 13$ | $b - 29$ |
| radix-4, twiddles | $(1 + 3 sqrt 2)(1 + 2^-15) < 2^2.4$ | $b - 12$ | $b - 28$ |

### Scan cost

The first scan is done together with the bit reversal. Each later scan reads
the block once. At 256 points, a q15 scan takes {cy}`fft q15 scan, 256`
cycles and the first radix-4 stage {cy}`fft q15 stage q = 1, 256` cycles.

## Rounding

Each stage output is the exact sum of its products, rounded once to nearest.
Ties round down, toward $-inf$. The fixed-point sums described in
[Exact sums](exact-sums.md) round ties up.

Ties down keep each stage inside its bound. Consider the first radix-4 stage
in q15 with the inputs 32767, -32768, 32767, -32768. Their sum is 131070.
The stage divides by 4, which gives 32767.5. Rounding the tie up gives
32768, which does not fit int16. Rounding it down gives 32767. No stage
output saturates or wraps.

Rounding ties down adds a small negative bias, which grows with the number
of stages. Over 2000 blocks of white noise, the mean error of
{c:func}`fd_cfft_q15` at DC is {acc}`cfft q15; flexdsp; 256 @ dc, real part`
LSB at 256 points and {acc}`cfft q15; flexdsp; 1024 @ dc, real part` LSB at
1024 points. The largest mean error at any other bin is
{acc}`cfft q15; flexdsp; 1024 @ other bins, largest` LSB at 1024 points.

## Differences from CMSIS-DSP

- `arm_cfft_q15` and `arm_cfft_q31` scale every stage by a fixed amount.
  The q15 output format is fixed per size, for example 9.7 at 256 points.
- A fixed scaling discards low bits of a quiet input. On white noise 40 dB
  below full scale at 256 points, the SNR is
  {acc}`cfft q15; flexdsp; 256; white, -40 dB @ snr db` dB for
  {c:func}`fd_cfft_q15` and
  {acc}`cfft q15; CMSIS-DSP; 256; white, -40 dB @ snr db` dB for
  `arm_cfft_q15`.
- CMSIS-DSP truncates. Its mean error at DC is
  {acc}`cfft q15; CMSIS-DSP; 256 @ dc, real part` LSB at 256 points and
  {acc}`cfft q15; CMSIS-DSP; 1024 @ dc, real part` LSB at 1024 points, in
  its own output format.

Cycle counts and SNR at other sizes are on [fd_fft](../dsp/fd_fft.md).

## Using the exponent

- {c:func}`fd_fft_rescale_q15` moves bins to another exponent, rounded and
  saturated. Rescaling to $log2 n$ gives $X / n$, the scaling of CMSIS-DSP.
- {c:func}`fd_fft_db_q15` returns $10 log10 |X|^2$ with e included.
- Two transforms can only be compared at the same exponent. Rescale the one
  with the smaller e to the larger e.
- {c:func}`fd_corr_fft_q15` returns an exponent in the same way.
  {c:func}`fd_autocorr_q15` returns an exponent that puts $r[0]$ in
  $[2^30, 2^31)$.
