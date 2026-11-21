# Exact sums

Many flexlib functions add up products: dot products, FIR filters, biquads,
convolution, correlation and matrix products. How a library handles that sum
decides how accurate the result is. flexlib keeps the whole sum exact, in an
integer accumulator wide enough for every partial sum, and rounds once at
the end. The result is then within 0.5 LSB of the exact value, for every
input, because nothing is lost before that last step.

This page explains how wide the accumulator has to be, how the result is
rounded and saturated, and where CMSIS-DSP does it differently. It applies to
every function above unless its own page says otherwise.

## Where a sum goes wrong

Take a q15 FIR filter. Each sample and each coefficient is an `int16_t`
standing for a fraction: 16384 is $16384 / 2^15$, which is 0.5. The product
of two q15 values is a Q30 integer of up to $2^30$, and a 32-tap filter adds
32 of them. The sum can grow far past what 32 bits hold, and a library has
three ways to deal with that:

- Shift each product down before adding it. The sum fits, but every shift
  drops low bits, and the error grows with the number of terms.
- Let the sum wrap. It is fast, but a large result loses its high bits and
  changes sign.
- Use an accumulator wide enough for any partial sum. Nothing is lost, and
  the only error is the final rounding.

flexlib does the third, and picks the narrowest accumulator that is safe so
that it costs as little as possible.

## How wide is wide enough

For inputs below 1 and coefficients h, every partial sum of $h_i x_i$ in q15
is at most $2^30 sum |h|$. The width therefore follows from $sum |h|$, the
sum of the coefficients' magnitudes:

| format | product | $sum \|h\| < 2$ | $sum \|h\| >= 2$ |
|---|---|---|---|
| q15 | Q30, 32 bits | 32-bit accumulator | 64-bit accumulator |
| q31 | Q62, 64 bits | 64-bit accumulator | 96-bit accumulator |

Most filters fall in the left column: a lowpass with a gain of 1 has
$sum |h|$ a little above 1. Filters with more gain take the wider
accumulator, at about twice the cycles.

The bound depends only on the coefficients, so it is worked out once, when
you set the filter up:

- {c:func}`fd_fir_init_q15` and {c:func}`fd_fir_init_q31` compute $sum |h|$
  with {c:func}`fd_fir_gain_q15` or {c:func}`fd_fir_gain_q31`, and pick the
  kernel. An interpolator picks one for each branch.
- {c:func}`fd_biquad_init_q15` and {c:func}`fd_biquad_init_q31` test each
  section. A section with $|b0| + |b1| + |b2| + |a1| + |a2| < 4$ runs in one
  accumulator. [Biquads](../dsp/fd_biquad.md) describes the others.
- {c:func}`fd_conv_q15` and {c:func}`fd_corr_q15` have no setup step. They
  test $max |a| sum |b|$ on each call.

The choice of kernel changes the cycle count, never the result.

## Matrix and vector products

The functions in [Vectors and matrices](../math/fm_linalg.md) sum three or
four products per output.

In q31, each product is a 64-bit integer, and four of them can add up past
the int64 range. The implementation keeps a separate sum of the products'
high words to notice when that happens. A sum outside the int64 range is at
least 2 in magnitude, outside q31 anyway, so the result saturates.

In float, each mantissa becomes an integer, and the products are aligned to
the largest exponent and added in 64 bits. The error is at most
$0.5 ULP + 2^-32 sum |a_i b_i|$, the second term coming from the alignment
shifts. A plain float loop rounds after every operation instead, and its
error grows with the number of terms. When terms cancel, as in
$10^8 + 1 - 10^8$, the plain loop can lose the answer altogether.

## Rounding

Results round to nearest. A value exactly halfway between two outputs, a
tie, rounds up, toward $+inf$. For a Q30 sum s and a q15 output:

```c
y = (s + (1 << 14)) >> 15;   /* arithmetic shift */
```

Truncating, leaving out the added half LSB, saves one instruction, but its
error lies in $(-1, 0]$ LSB with a mean of $-0.5$ LSB. That mean is a
constant offset in the output, and anything downstream that integrates or
averages adds it up. Over $10^6$ FIR outputs the mean error is
{acc}`FIR q15, 32 taps; flexdsp @ bias` LSB for {c:func}`fd_fir_q15`, and
{acc}`FIR q15, 32 taps; CMSIS-DSP @ bias` LSB for `arm_fir_q15`, which
truncates.

The FFT stages are the exception: they round ties down.
[Block floating point](block-floating-point.md) explains why.

## Saturation

After rounding, the result is clamped to the output range,
$[-2^15, 2^15 - 1]$ for q15 and $[-2^31, 2^31 - 1]$ for q31. No result
wraps. A filter driven past full scale then clips, as an analog stage would,
instead of jumping to the opposite sign.

## Differences from CMSIS-DSP

Here is how the corresponding CMSIS-DSP functions behave, as built for the
Cortex-M3:

- `arm_fir_q15` sums in 64 bits, truncates and saturates.
- `arm_fir_q31` sums in 64 bits and truncates, but does not saturate: a
  result outside q31 wraps.
- `arm_dot_prod_q31` shifts each product right by 14 bits before adding, so
  its error grows with the number of terms.
- `arm_mat_mult_q31` sums in 64 bits without saturating the partial sums.
  Its documentation asks you to scale the inputs down by $log2(n)$ bits to
  avoid overflow.

The cycle counts and measured errors are on the module pages:
[FIR filters](../dsp/fd_fir.md), [Vectors](../dsp/fd_vec.md) and
[Vectors and matrices](../math/fm_linalg.md).
