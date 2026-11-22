# flexnn

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

flexnn runs small quantised neural networks on Cortex-M: int8 inference with
the arithmetic of TFLite's int8 models. A network you train and quantise on a
PC gives the same integers on the board. It is in development and not part
of a release yet. What is here so far: fully connected layers, activation
functions as tables, softmax and argmax, and the step every layer ends with,
turning its int32 sums into int8 outputs.

## Fully connected layers (`fnn_dense.h`)

A fully connected layer multiplies each input, less the input zero point, by
a weight per output, adds a bias and requantises the sum. Here a layer has
three inputs, two outputs and an output scale of 1/64:

```c
#include <stdio.h>

#include "fnn_dense.h"
#include "fnn_quant.h"

int main(void) {
  /* two outputs from three inputs, the weights in TFLite's row order */
  static const int8_t w[2 * 3] = {10, -20, 30, 5, 5, 5};
  static const int32_t b[2] = {100, -50};
  const int32_t zx = 3; /* input zero point */

  int32_t bias[2], mult;
  int8_t rsh;
  fnn_fold_bias_s8(w, 3U, 2U, b, zx, bias);
  if (fnn_quant_from_scale(1.0 / 64.0, &mult, &rsh) != FNN_OK) {
    return 1;
  }
  const FNN_Dense layer = {w, bias, 3U, 2U, 0U, {&mult, &rsh, 1U}, {0, -128, 127}};

  const int8_t x[3] = {13, 23, 33};
  int8_t y[2];
  fnn_dense_s8(&layer, x, y);
  printf("%d %d\n", y[0], y[1]);
  return 0;
}
```

Which prints:

```text
11 4
```

The inputs less their zero point are 10, 20 and 30. The first output's sum is
10 × 10 − 20 × 20 + 30 × 30 + 100 = 700, and 700 / 64 rounds to 11. The second
is 5 × 60 − 50 = 250, which gives 4. `fnn_fold_bias_s8` moves the zero point
into the biases once, at setup: the kernel then multiplies the raw inputs and
never subtracts anything per input.

The weights can stay in TFLite's order, rows of `n_in`, as above. For speed,
reorder them once with `fnn_dense_pack_s8` and set the layer's `packed` to 1:
the kernel then reads eight rows' weights with one load and runs about a fifth
faster. The packed order is flexnn's own and may change between versions:
keep the rows and pack them again, at startup into RAM or ahead of time into a
`const` array. Pass the rows, not the packed weights, to `fnn_fold_bias_s8`.

Cycles on an STM32F103 at 72 MHz (`-O2`, weights in flash), against CMSIS-NN
8.0.0's `arm_fully_connected_s8` built the same way:

| layer | flexnn, packed | flexnn, rows | CMSIS-NN |
| --- | --: | --: | --: |
| 256 inputs, 64 outputs | 85489 | 101317 | 123988 |
| 64 inputs, 12 outputs | 4156 | 4936 | 5492 |

That is 5.2 cycles per multiply-accumulate packed, against CMSIS-NN's 7.6. On
the Cortex-M3 the multiply-accumulate itself takes 2 of those cycles, and
splitting four weights out of a word takes one more per weight.

## Activation functions (`fnn_lut.h`)

An int8 activation such as tanh or the logistic function has only 256
possible inputs. You can compute all 256 outputs once, at startup, and the
layer then costs one table lookup per value. Here tanh takes inputs at a
scale of 1/16 to outputs at a scale of 1/128, the output scale TFLite uses
for tanh:

```c
#include <math.h>
#include <stdio.h>

#include "fnn_lut.h"

int main(void) {
  /* tanh from inputs at a scale of 1/16 to outputs at 1/128, TFLite's */
  static int8_t lut[256];
  if (fnn_lut_s8_build(tanh, 1.0 / 16.0, 0, 1.0 / 128.0, 0, lut) != FNN_OK) {
    return 1;
  }

  int8_t x[5] = {-32, -8, 0, 8, 32};
  fnn_lut_s8(x, x, 5U, lut);
  for (int i = 0; i < 5; i++) {
    printf("%d ", x[i]);
  }
  printf("\n");
  return 0;
}
```

Which prints:

```text
-123 -59 0 59 123
```

The inputs stand for -2, -0.5, 0, 0.5 and 2. Their tanh, -0.964 to 0.964,
is -123.4 to 123.4 in steps of 1/128, and each rounds to the nearest integer.
`fnn_lut_s8_build` takes any `double` function of one `double`: `tanh` from
math.h, or one of your own for the logistic function, hard-swish or leaky
ReLU. It works in double precision, which a Cortex-M3 runs in software: build
tables at startup or ahead of time, not per inference. The output can
overwrite the input, as it does here.

Every entry is the exact value rounded once, ties up, and clamped to int8.
TFLite computes some of these functions in fixed point instead, and its
outputs can then differ by 1. When you need TFLite's outputs exactly, run the
operator in the interpreter on all 256 inputs and use what comes out as the
table, at index `(uint8_t)q` for input q.

A table applied to 1024 values takes 3434 cycles on an STM32F103 at 72 MHz
(`-O2`), 3.4 cycles per value.

## Softmax and argmax (`fnn_softmax.h`)

A classifier ends with one score per class. To report the class, take the
index of the largest score with `fnn_argmax_s8`. To report how sure the model
is, turn the scores into probabilities with `fnn_softmax_s8`. Here four scores
at a scale of 1/16 go through both:

```c
#include <stdio.h>

#include "fnn_softmax.h"

int main(void) {
  /* four class scores at a scale of 1/16, beta 1 */
  static uint32_t exp_tab[256];
  if (fnn_softmax_s8_build(1.0, 1.0 / 16.0, exp_tab) != FNN_OK) {
    return 1;
  }

  const int8_t score[4] = {20, 52, 36, 4};
  int8_t p[4];
  fnn_softmax_s8(score, p, 4U, exp_tab);
  for (int i = 0; i < 4; i++) {
    printf("%d ", p[i]);
  }
  printf("\nclass %u\n", (unsigned)fnn_argmax_s8(score, 4U));
  return 0;
}
```

Which prints:

```text
-106 37 -67 -120
class 1
```

The scores stand for 1.25, 3.25, 2.25 and 0.25. Their softmax is 0.087, 0.644,
0.237 and 0.032, and the outputs use TFLite's int8 softmax format: a scale of
1/256 and a zero point of -128. The first output, -106, is 22/256, and 0.087 ×
256 = 22.3 rounds to 22. `fnn_argmax_s8` returns 1, the index of 52. Of equal
scores it picks the first, as TFLite does.

`fnn_softmax_s8_build` fills a 1 KB table of exponentials for one input scale
and beta, in floating point, at startup. Each output is then the table's
exponentials divided by their sum, rounded once: within half a step of the
exact softmax, plus at most $(n + 1) 2^{-32}$. The one exception is a
probability above 255.5/256, which the format cannot hold and which gives 127.
The output can overwrite the scores.

Cycles on an STM32F103 at 72 MHz (`-O2`), against CMSIS-NN 8.0.0's
`arm_softmax_s8` built the same way:

| scores | `fnn_softmax_s8` | CMSIS-NN | `fnn_argmax_s8` |
| --- | --: | --: | --: |
| 12 | 602 | 8159 | 122 |
| 256 | 11436 | - | 2319 |

The division per output is a 32-bit `udiv` on a shortened sum. Its quotient is
right or one too small, and the remainder shows which: a 64-bit check runs
only when the remainder is too close to tell, at most about once in 8000
outputs.

## Requantisation (`fnn_quant.h`)

A layer multiplies int8 inputs by int8 weights and adds the products in
int32. Its output is int8 at its own scale, and each sum has to be multiplied
by a real factor (input scale times weight scale over output scale), rounded,
moved by the output's zero point and clamped. Here four sums go to an output
with a scale of 1/256 and a zero point of -10:

```c
#include <stdio.h>

#include "fnn_quant.h"

int main(void) {
  /* four int32 sums from a layer, and the layer's output scale of 1/256 */
  static const int32_t acc[4] = {384, -384, 32000, -40000};
  int32_t m;
  int8_t rsh;
  if (fnn_quant_from_scale(1.0 / 256.0, &m, &rsh) != FNN_OK) {
    return 1;
  }
  const FNN_Quant q = {&m, &rsh, 1U}; /* one multiplier for the tensor */
  const FNN_Out o = {-10, -128, 127}; /* zero point -10, the full int8 range */

  int8_t y[4];
  fnn_requant_s8(acc, y, 4U, &q, &o);
  for (int i = 0; i < 4; i++) {
    printf("%6ld -> %4d\n", (long)acc[i], y[i]);
  }
  return 0;
}
```

Which prints:

```text
   384 ->   -8
  -384 ->  -11
 32000 ->  115
-40000 -> -128
```

`fnn_quant_from_scale` turns the real factor into a 31-bit multiplier and a
shift the way TFLite does: a layer you build by hand gets TFLite's numbers.
384 / 256 is 1.5 and rounds to 2, which the zero point moves to -8.
-384 / 256 is -1.5, exactly between -2 and -1: flexnn's default rounds ties
up, to -1, and the zero point makes it -11. -40000 / 256 is far below the
int8 range and clamps to -128. A per-channel layer passes one multiplier per
channel and sets `q.n` to the channel count. The clamp is also where a fused
ReLU or ReLU6 goes: it raises `lo` to the zero point, or lowers `hi`.

## Rounding

Three integer arithmetics are in use for int8 networks, and they differ by 1
in some outputs. You pick one for the whole build with `FNN_ROUNDING`:

| `FNN_ROUNDING` | rounding | matches |
| --- | --- | --- |
| `FNN_ROUND_UP` (default) | once, ties up | CMSIS-NN built with `CMSIS_NN_USE_SINGLE_ROUNDING` |
| `FNN_ROUND_AWAY` | once, ties away from zero | the TFLite (LiteRT) interpreter's reference kernels |
| `FNN_ROUND_GEMMLOWP` | twice (gemmlowp) | CMSIS-NN's default build, TFLite Micro on Cortex-M |

In the example only -384 changes: `FNN_ROUND_AWAY` and `FNN_ROUND_GEMMLOWP`
give -12. Rounding once from the exact product is the flexlib rule and the
default. Use `FNN_ROUND_AWAY` when you check the board's outputs against the
TFLite interpreter on your PC. Its optimised kernels, which it uses unless you
ask for the reference ones, round differently again (half to even, through
float). Compare against the reference kernels.

Every input gives a defined result. Where TFLite's arithmetic would overflow
int32 (scales of 1/2 and above with large sums), flexnn saturates.

## How it was checked

Fully connected layers against a reference written in TFLite's own terms, with
the zero point subtracted per input, in both layouts and every rounding mode:
random layers of 0 to 256 inputs and 1 to 70 outputs, weights and inputs at
their extremes, per-tensor and per-channel scales, fused clamps, and a folded
bias that wraps past int32 while the true sum fits. Four layers, up to 256
inputs and 70 outputs, against the TFLite interpreter, bit for bit.

Activation tables: every entry of tanh, logistic, hard-swish, exp and
exact-tie tables within half a step of the exact value, ties up, with clamping
at both ends and NaN and infinite results. The kernel at every length to 40
and up to 1030, every alignment, and in place, with guard bytes around the
output. Against the TFLite interpreter for LOGISTIC, TANH, HARD_SWISH and
LEAKY_RELU: the exact tables for LOGISTIC and TANH equal TFLite's on every
entry, the other two differ on some entries by 1, and a table read back from
the interpreter reproduces it on every output.

Softmax against an exact model of its own arithmetic and against the true
softmax, at 1 to 1100 scores and five input scales: equal scores, the
certain case, the far end of the table, sums past 2^40, and 100,000 sums built
to sit just off a rounding tie, where the cheap division is closest to wrong.
Against the TFLite interpreter, 58,000 outputs at four scales and betas, all
equal. Argmax against a plain loop at every length to 300 and against TFLite
on 1500 runs, half of them full of ties.

Requantisation: every int32 sum, for 15 multiplier and shift pairs at the ends of their
ranges, in each rounding mode, against an exact 64-bit model. Random sums,
multipliers and every shift against a second model written in TFLite's and
gemmlowp's own terms. `fnn_quant_from_scale` against TFLite's
`QuantizeMultiplier` on 100,000 random scales and the edge cases (scales that
round up to the next power of two, scales too small or too large to
represent). [docs/testing.md](../docs/testing.md) lists the commands.
