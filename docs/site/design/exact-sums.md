# Exact sums

Several functions compute a sum of products: dot products, FIR filters,
biquads, convolution, correlation and matrix products. In flexlib these
functions compute the sum exactly in an integer accumulator. The exact sum is
rounded once to the output format and then saturated.

This page describes the accumulator width, the rounding and the saturation.
It applies to every function listed above unless its page says otherwise.

## Terms

Q format
: A fixed-point number stored as an integer with an implied scale. A q15
  value is an `int16_t` that represents $x / 2^15$. A q31 value is an
  `int32_t` that represents $x / 2^31$.

LSB
: One unit of the integer, the smallest step of the format. One LSB of q15
  is $2^-15$.

Accumulator
: The integer that holds the running sum while the products are added.

Saturation
: A result outside the range of the output format is replaced by the
  nearest value in range. The alternative is wrapping, where the result
  loses its high bits and changes sign.

## Error bound

The accumulator is wide enough to hold every partial sum without loss. The
only error is introduced by the final rounding. The result is therefore
within 0.5 LSB of the exact value. This holds for every input, because it
follows from the construction. The tests check it against a 128-bit
reference.

## Accumulator width

The product of two q15 values is an integer of at most $2^30$ in magnitude.
For coefficients h and input x, every partial sum of $h_i x_i$ is bounded by
$2^30 sum |h|$. The required width follows from $sum |h|$:

| format | product | $sum \|h\| < 2$ | $sum \|h\| >= 2$ |
|---|---|---|---|
| q15 | Q30, 32 bits | 32-bit accumulator | 64-bit accumulator |
| q31 | Q62, 64 bits | 64-bit accumulator | 96-bit accumulator |

### Kernel selection

The bound depends only on the coefficients. It is computed when the filter is
initialised:

- {c:func}`fd_fir_init_q15` and {c:func}`fd_fir_init_q31` compute $sum |h|$
  with {c:func}`fd_fir_gain_q15` or {c:func}`fd_fir_gain_q31` and select the
  kernel. An interpolator selects a kernel for each branch.
- {c:func}`fd_biquad_init_q15` and {c:func}`fd_biquad_init_q31` test each
  section. A section with $|b0| + |b1| + |b2| + |a1| + |a2| < 4$ runs in one
  accumulator. The handling of other sections is described on
  [fd_biquad](../dsp/fd_biquad.md).
- {c:func}`fd_conv_q15` and {c:func}`fd_corr_q15` have no initialisation.
  They test $max |a| sum |b|$ on each call.

The kernel selection does not change the result. It changes only the cycle
count.

## Matrix and vector products

The functions in [fm_linalg](../math/fm_linalg.md) sum three or four
products per output.

In q31, each product is a 64-bit integer. The sum of four products can
exceed the int64 range. The implementation keeps a separate sum of the
products' high words to detect this case. A sum outside the int64 range is at least 2
in magnitude, which is outside q31. The result saturates.

In float, each mantissa is converted to an integer. The products are aligned
to the largest exponent and added in 64 bits. The error is at most
$0.5 ULP + 2^-32 sum |a_i b_i|$. The second term comes from the alignment
shifts. A plain float loop rounds after each operation instead, and its
error grows with the number of terms.

## Rounding

Results round to nearest. A value exactly halfway between two outputs (a
tie) rounds toward $+inf$. For a Q30 sum s and a q15 output, the rounding is:

```c
y = (s + (1 << 14)) >> 15;   /* arithmetic shift */
```

Truncation omits the added half LSB. Its error lies in $(-1, 0]$ LSB, with
a mean of $-0.5$ LSB. The mean is a constant offset in the output. An integrator
or a long average downstream accumulates it. Measured over $10^6$ FIR
outputs, the mean error is {acc}`FIR q15, 32 taps; flexdsp @ bias` LSB for
{c:func}`fd_fir_q15` and {acc}`FIR q15, 32 taps; CMSIS-DSP @ bias` LSB for
`arm_fir_q15`.

The FFT stages are an exception. They round ties down. The reason is given in
[Block floating point](block-floating-point.md).

## Saturation

After rounding, the result is clamped to the output range: $[-2^15, 2^15 - 1]$
for q15 and $[-2^31, 2^31 - 1]$ for q31. No result wraps.

## Differences from CMSIS-DSP

The corresponding CMSIS-DSP functions, as built for the Cortex-M3:

- `arm_fir_q15` sums in 64 bits, truncates and saturates.
- `arm_fir_q31` sums in 64 bits, truncates, and does not saturate. A result
  outside q31 wraps.
- `arm_dot_prod_q31` shifts each product right by 14 bits before adding. Its
  error grows with the number of terms.
- `arm_mat_mult_q31` sums in 64 bits without saturating the intermediate
  sums. Its documentation asks for the inputs to be scaled down by
  $log2(n)$ bits to avoid overflow.

Cycle counts and measured errors for each function are on the module pages:
[fd_fir](../dsp/fd_fir.md), [fd_vec](../dsp/fd_vec.md),
[fm_linalg](../math/fm_linalg.md).
