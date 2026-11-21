# Block floating point

A fixed-point FFT has a problem a float FFT does not: its values grow as they
pass through the stages, and the integers they live in have a fixed size.
The usual answer is to scale every stage down by a fixed amount, which is
safe but throws away the low bits of a quiet signal. The FFTs in
[FFT](../dsp/fd_fft.md) use block floating point instead. All values in the
block share one exponent, each stage is scaled by what the data in front of
it actually needs, and the transform returns the exponent it ended with.

This page explains the result format, how each stage is scaled and rounded,
and how to use the exponent.

## The result and its exponent

Each transform works in place and returns an exponent e:

```c
int32_t e = fd_cfft_q15(x, 256);
/* bin k is (x[k].re + i x[k].im) * 2^e, in units of the input's LSB */
```

The result is the plain DFT sum, with no factor $1 / n$. The inverse
transforms include the $1 / n$. A block of zeros gives zeros and $e = 0$.

So a value v in the result stands for $v 2^e$. With $e = 3$, a bin holding
1000 means 8000 input LSBs. Two spectra can only be compared at the same
exponent; the last section shows how.

## How each stage is scaled

Before each stage, a scan finds b, the number of bits the largest magnitude
in the block needs. A block whose largest value is 1000 needs 10 bits plus
the sign, so in q15 it has 5 bits of headroom ({c:func}`fd_headroom_q15`
returns that number). Each kind of stage can grow its largest value by a
known factor, so the stage shifts its input by just enough to keep the
output inside the integer type:

| stage | growth bound | q15 shift | q31 shift |
|---|---|---|---|
| radix-2 | 2 | $b - 14$ | $b - 30$ |
| radix-4, no twiddles | 4 | $b - 13$ | $b - 29$ |
| radix-4, twiddles | $(1 + 3 sqrt 2)(1 + 2^-15) < 2^2.4$ | $b - 12$ | $b - 28$ |

A positive shift is to the right and a negative one to the left. A loud
block shifts right, as a fixed-scaling FFT would. A quiet block shifts
left, and gains back bits that a fixed scaling would have dropped. The
exponent e is the sum of the shifts over all the stages. The twiddle
factors are the complex constants $exp(-2 pi i k / n)$ that a stage
multiplies some of its inputs by. The first radix-4 stage has none, which is
why it grows less.

The scans are cheap. The first one rides along with the bit reversal, and
each later one reads the block once. At 256 points, a q15 scan takes
{cy}`fft q15 scan, 256` cycles, against {cy}`fft q15 stage q = 1, 256`
cycles for the first radix-4 stage.

## Rounding ties down

Each stage output is the exact sum of its products, rounded once to nearest.
Ties round down, toward $-inf$, where the rest of flexlib rounds ties up
(see [Exact sums](exact-sums.md)).

Rounding ties down is what keeps every stage inside its bound. Take the
first radix-4 stage in q15 with the inputs 32767, -32768, 32767, -32768.
Their sum is 131070, and the stage divides by 4, which gives 32767.5.
Rounded up, that tie would be 32768, which does not fit an `int16_t`.
Rounded down it is 32767. So no stage output ever saturates or wraps.

The price is a small negative bias that grows with the number of stages.
Over 2000 blocks of white noise, the mean error of {c:func}`fd_cfft_q15` at
DC is {acc}`cfft q15; flexdsp; 256 @ dc, real part` LSB at 256 points, and
{acc}`cfft q15; flexdsp; 1024 @ dc, real part` LSB at 1024 points. The
largest mean error at any other bin is
{acc}`cfft q15; flexdsp; 1024 @ other bins, largest` LSB at 1024 points.

## Differences from CMSIS-DSP

`arm_cfft_q15` and `arm_cfft_q31` scale every stage by a fixed amount, so
their output format is fixed for each size, for example 9.7 at 256 points.
That discards the low bits of a quiet input. On white noise 40 dB below full
scale at 256 points, the SNR is
{acc}`cfft q15; flexdsp; 256; white, -40 dB @ snr db` dB for
{c:func}`fd_cfft_q15` and
{acc}`cfft q15; CMSIS-DSP; 256; white, -40 dB @ snr db` dB for
`arm_cfft_q15`.

CMSIS-DSP truncates rather than rounding. Its mean error at DC is
{acc}`cfft q15; CMSIS-DSP; 256 @ dc, real part` LSB at 256 points and
{acc}`cfft q15; CMSIS-DSP; 1024 @ dc, real part` LSB at 1024 points, in its
own output format. [FFT](../dsp/fd_fft.md) has the cycle counts and the SNR
at other sizes.

## Using the exponent

- {c:func}`fd_fft_rescale_q15` moves bins to another exponent, rounded and
  saturated. Rescaling to $log2 n$ gives $X / n$, CMSIS-DSP's scaling, if
  you want results in that form.
- {c:func}`fd_fft_db_q15` returns $10 log10 |X|^2$ with e already included,
  so 0 dB is full scale whatever scaling the transform chose.
- To compare two transforms, bring them to the same exponent: rescale the
  one with the smaller e to the larger e.
- {c:func}`fd_corr_fft_q15` returns an exponent in the same way, and
  {c:func}`fd_autocorr_q15` returns one that puts $r[0]$ in
  $[2^30, 2^31)$.
