# flexmath

A Cortex-M3 has no floating-point unit. Every float operation becomes a call
into libgcc: a multiply takes {cy}`float multiply` cycles and a divide
{cy}`float divide`, and newlib's `sinf` takes {cy}`sinf`. flexmath gives you
two ways around that. The fixed-point functions work on fractions held in
integers, in Q15, Q31 and Q16.16. The float functions take and return
`float` as usual, but compute with integer operations inside, so code
written with floats runs several times faster without changing its types.

## Conventions

Fixed-point results saturate: a result past the largest value comes back as
the largest value, never wrapped round to a negative one. Errors are given in
LSBs of the output format, or in ULPs for float, against a `long double`,
`double` or exact reference. Each function's header states its bound, and
the module pages show how it was measured.

Angles are binary: a full turn is 2^16 for the Q15 functions and 2^32 for
the Q31 ones. An angle held in an unsigned integer then wraps round by
itself, which is what a phase accumulator needs.

| header | what it is for |
|---|---|
| [`fm_fixed.h`](fm_fixed.md) | the Q formats, saturating arithmetic, a Q62 accumulator, float conversion |
| [`fm_sqrt.h`](fm_sqrt.md) | integer square roots, roots in Q15, Q31 and Q16.16, 1/sqrt in Q16.16 |
| [`fm_div.h`](fm_div.md) | division in Q15, Q31 and Q16.16, and 1/x |
| [`fm_trig.h`](fm_trig.md) | sine and cosine of binary angles |
| [`fm_atan.h`](fm_atan.md) | atan2, asin and acos |
| [`fm_exp.h`](fm_exp.md) | logarithms, dB, exponentials and powers in Q16.16 |
| [`fm_float.h`](fm_float.md) | sqrt, reciprocals, trigonometry, logarithms and exponentials on `float`, without an FPU |
| [`fm_linalg.h`](fm_linalg.md) | dot and cross products, 3x3 and 4x4 matrices, in q31 and float |
| [`fm_quat.h`](fm_quat.md) | quaternions for attitude: product, normalise, rotate, integrate gyro rates |
| [`fm_pid.h`](fm_pid.md) | PID controllers with output limits and anti-windup |

flexmath brings flexutils with it. Set `FLEXLIB_LIBS := math`, as
[Get started](../get-started.md) shows.

## Options

`FM_LUT_IN_RAM`
: Puts flexmath's tables (2.8 KB) in RAM. A call whose table is not in the
  flash cache is then faster: {c:func}`fm_sin_q15` takes 368 cycles instead
  of 480.

`FX_ASSERT_FAIL(file, line)`, `FX_DISABLE_ASSERT`
: By default a failed argument check traps. Define the first to report it
  your own way, or the second to remove the checks.

```{toctree}
:hidden:

fm_fixed
fm_sqrt
fm_div
fm_trig
fm_atan
fm_exp
fm_float
fm_linalg
fm_quat
fm_pid
```
