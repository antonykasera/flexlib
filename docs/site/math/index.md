# flexmath

Fixed-point arithmetic and functions in Q15, Q31 and Q16.16 for Cortex-M
parts without an FPU. C11, no allocation; `flexmath.mk` brings flexutils
with it.

## Conventions

- Results saturate instead of wrapping.
- Errors are in LSBs of the output format, against a `long double` or exact
  reference. Each function's bound is in its header.
- Angles are binary: a full turn is 2^16 for Q15 functions and 2^32 for Q31.

## Modules

| header | contents |
|---|---|
| [`fm_fixed.h`](fm_fixed.md) | Q formats, saturating add, subtract, multiply, Q62 accumulator, float conversion |
| [`fm_sqrt.h`](fm_sqrt.md) | integer square roots, sqrt in Q15 / Q31 / Q16.16, 1/sqrt in Q16.16 |
| [`fm_div.h`](fm_div.md) | division in Q15 / Q31 / Q16.16, 1/x in Q16.16 |
| [`fm_trig.h`](fm_trig.md) | sine and cosine of binary angles |
| [`fm_atan.h`](fm_atan.md) | atan2, asin, acos |
| [`fm_exp.h`](fm_exp.md) | log2, ln, log10, dB, 2^x, e^x, x^y in Q16.16 |

## Options

`FM_LUT_IN_RAM`
: Puts the tables (2.8 KB) in `.data`. After a flash cache flush,
  {c:func}`fm_sin_q15` takes 368 cycles instead of 480.

`FX_ASSERT_FAIL(file, line)`, `FX_DISABLE_ASSERT`
: Argument checks trap by default.

```{toctree}
:hidden:

fm_fixed
fm_sqrt
fm_div
fm_trig
fm_atan
fm_exp
```
