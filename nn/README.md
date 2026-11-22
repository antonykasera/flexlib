# flexnn

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

flexnn runs small quantised neural networks on Cortex-M: int8 inference with
the arithmetic of TFLite's int8 models. A network you train and quantise on a
PC gives the same integers on the board. It is in development and not part
of a release yet. What is here so far: fully connected, convolution and
depthwise layers, pooling and mean, add and concatenation, activation
functions as tables, softmax and argmax, conversions in and out of int8, and
the step every layer ends with, turning its int32 sums into int8 outputs.

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
the kernel then reads eight rows' weights with one load and runs 14 to 27%
faster. The packed order is flexnn's own and may change between versions:
keep the rows and pack them again, at startup into RAM or ahead of time into a
`const` array. Pass the rows, not the packed weights, to `fnn_fold_bias_s8`.

Cycles on an STM32F103 at 72 MHz (`-O2`), against CMSIS-NN 8.0.0's
`arm_fully_connected_s8` built the same way. The board's flash cache holds 16
to 32 KB: a layer whose weights fit is timed with them in the cache, and the
16 KB layer from a flushed cache, its weights read from flash:

| layer | weights | flexnn, packed | flexnn, rows | CMSIS-NN |
| --- | --- | --: | --: | --: |
| 64 inputs, 12 outputs | in the cache | 4156 | 4936 | 5492 |
| 128 inputs, 64 outputs | in the cache | 37417 | 47457 | 53136 |
| 256 inputs, 64 outputs | from flash | 131903 | 150677 | 164891 |

With its weights in the cache the packed kernel takes 4.6 cycles per
multiply-accumulate, against CMSIS-NN's 6.5. On the Cortex-M3 the
multiply-accumulate itself takes 2 of those cycles, and splitting four weights
out of a word takes one more per weight. From flash it takes 8.0, against 10.1.

## Convolution (`fnn_conv.h`)

A convolution slides a small filter over an image or a signal, and each output
pixel is a fully connected layer over the patch under the filter. Here a 3 x 3
filter that responds to vertical edges runs over a 4 x 4 image with SAME
padding, which keeps the output the size of the input:

```c
#include <stdio.h>

#include "fnn_conv.h"
#include "fnn_dense.h"
#include "fnn_quant.h"

int main(void) {
  /* a 4 x 4 image, one channel, and a 3 x 3 filter that finds vertical edges */
  static const int8_t img[4 * 4] = {0, 0, 9, 9, 0, 0, 9, 9,
                                    0, 0, 9, 9, 0, 0, 9, 9};
  static const int8_t w[1 * 3 * 3 * 1] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
  int32_t bias[1], mult;
  int8_t rsh;
  fnn_fold_bias_s8(w, 9U, 1U, NULL, 0, bias);
  if (fnn_quant_from_scale(1.0 / 4.0, &mult, &rsh) != FNN_OK) {
    return 1;
  }
  /* SAME padding: one row above and one column to the left */
  const FNN_Conv layer = {w,  bias, {4U, 4U, 1U}, {4U, 4U, 1U}, 3U, 3U,
                          1U, 1U,   1U,           1U,           0U, 0,
                          {&mult, &rsh, 1U},      {0, -128, 127}};
  if (fnn_conv_check(&layer) != FNN_OK) {
    return 1;
  }
  int8_t scratch[FNN_CONV_SCRATCH(3, 3, 1)], out[4 * 4];
  fnn_conv_s8(&layer, img, out, scratch);
  for (int r = 0; r < 4; r++) {
    printf("%3d %3d %3d %3d\n", out[4 * r], out[4 * r + 1], out[4 * r + 2],
           out[4 * r + 3]);
  }
  return 0;
}
```

Which prints:

```text
  0   7   7  -7
  0   9   9  -9
  0   9   9  -9
  0   7   7  -7
```

In the middle rows each window sees 0, 0 and 9 across, which the filter sums
to 9 + 2 x 9 + 9 = 36, and 36 / 4 = 9. At the top the window overhangs the image
and the padding row adds nothing: 27 / 4 = 6.75 rounds to 7. On the right the
padding column reads as 0 and the sum turns negative.

Tensors are NHWC, as in TFLite, and the filter keeps TFLite's
`[c_out][kh][kw][c_in]` order. That is a dense layer's rows of kh kw c_in
inputs, which is why `fnn_fold_bias_s8` takes it as it is, and why
`fnn_dense_pack_s8` packs it for the same faster kernel. Padded taps read the
input zero point, and with the folded bias they add exactly nothing, as in
TFLite. The kernel copies a window into the scratch only where it overhangs the
input or spans more than one run of memory: a 1 x 1 filter, or a 1-row filter
over a signal, reads the input in place. `fnn_conv_check` tells you whether a
layer's shapes, strides and padding fit together.

Cycles on an STM32F103 at 72 MHz (`-O2`, weights in the flash cache), against
CMSIS-NN 8.0.0 built the same way, with a multiplier per output channel:

| layer | flexnn, packed | flexnn, rows | CMSIS-NN |
| --- | --: | --: | --: |
| 1 x 1, 64 -> 64 channels, 5 x 5 | 519941 | 649601 | 591662 |
| 3 x 3, 8 -> 16 channels, 12 x 12, SAME | 970973 | 1179952 | 1135991 |
| 5 taps, 16 -> 32 channels, 64 long | 802694 | 1007647 | 979578 |

Packed, that is 4.9 to 5.9 cycles per multiply-accumulate, 12 to 18% fewer
cycles than CMSIS-NN. In TFLite's order the filter takes 3 to 10% more than
CMSIS-NN: pack it when you can.

## Depthwise convolution (`fnn_dwconv.h`)

A depthwise layer filters each channel on its own, with a filter of its own,
where a convolution mixes every input channel into every output. It is how
small vision and audio models keep their cost down. Here two channels of a
3 x 3 image get different filters:

```c
#include <stdio.h>

#include "fnn_dwconv.h"
#include "fnn_quant.h"

int main(void) {
  /* a 3 x 3 image with two channels, stored pixel by pixel (NHWC) */
  static const int8_t img[3 * 3 * 2] = {1, 10, 2, 20, 3, 30,
                                        4, 40, 5, 50, 6, 60,
                                        7, 70, 8, 80, 9, 90};
  /* channel 0 sums its window; channel 1 keeps only the centre */
  static const int8_t w[3 * 3 * 2] = {1, 0, 1, 0, 1, 0,
                                      1, 0, 1, 1, 1, 0,
                                      1, 0, 1, 0, 1, 0};
  int32_t bias[2], mult;
  int8_t rsh;
  fnn_dwconv_fold_bias_s8(w, 9U, 2U, NULL, 0, bias);
  if (fnn_quant_from_scale(1.0, &mult, &rsh) != FNN_OK) {
    return 1;
  }
  const FNN_DwConv layer = {w,  bias, {3U, 3U, 2U}, {1U, 1U, 2U}, 3U, 3U,
                            1U, 1U,   0U,           0U,           0,
                            {&mult, &rsh, 1U},      {0, -128, 127}};
  if (fnn_dwconv_check(&layer) != FNN_OK) {
    return 1;
  }
  int8_t out[2];
  fnn_dwconv_s8(&layer, img, out);
  printf("%d %d\n", out[0], out[1]);
  return 0;
}
```

Which prints:

```text
45 50
```

Channel 0 sums its nine pixels, 1 + 2 + ... + 9 = 45, and channel 1 keeps the
centre, 50. The filter is TFLite's `[kh][kw][c_out]`, channel-minor like the
image, and `fnn_dwconv_fold_bias_s8` folds the zero point over its taps. With a
channel multiplier m, `out.c` is m times `in.c` and output channel c reads
input channel c / m. Padding reads the input zero point, as in TFLite.

Cycles on an STM32F103 at 72 MHz (`-O2`), against CMSIS-NN 8.0.0 built the same
way, with a multiplier per channel:

| layer | flexnn | CMSIS-NN |
| --- | --: | --: |
| 3 x 3, 64 channels, 5 x 5, SAME | 299979 | 470046 |
| 3 x 3, 64 channels, 7 x 7 to 5 x 5, VALID | 244547 | - |

That is about a third fewer cycles than CMSIS-NN's faster depthwise kernel, but
still 17 to 21 cycles per multiply-accumulate. The work the kernel does per
group of four channels around its multiplies costs more than the multiplies,
and that is where the next improvement lies.

## Pooling and mean (`fnn_pool.h`)

A pooling layer shrinks an image by keeping one value per window, the largest
or the average, channel by channel. A mean layer averages each channel over
the whole image, which is how a classifier turns its last feature map into one
value per channel. Here a 4 x 4 image goes through 2 x 2 windows, then a mean:

```c
#include <stdio.h>

#include "fnn_pool.h"

int main(void) {
  /* a 4 x 4 image with one channel */
  static const int8_t img[4 * 4] = {1,  2,  3, 4,  5,  6,  7,  8,
                                    -1, -2, 9, 10, -3, -4, 11, 12};
  /* 2 x 2 windows, stride 2, no padding, no clamp */
  const FNN_Pool pool = {{4U, 4U, 1U}, {2U, 2U, 1U}, 2U, 2U, 2U, 2U,
                         0U,           0U,           -128, 127};
  if (fnn_pool_check(&pool) != FNN_OK) {
    return 1;
  }
  int8_t mx[4], av[4];
  fnn_maxpool_s8(&pool, img, mx);
  fnn_avgpool_s8(&pool, img, av);
  printf("max %d %d %d %d\n", mx[0], mx[1], mx[2], mx[3]);
  printf("avg %d %d %d %d\n", av[0], av[1], av[2], av[3]);

  /* the mean of the whole image, at an output scale half the input's */
  FNN_Mean mean = {{4U, 4U, 1U}, 0, 0, 0, {0, -128, 127}};
  if (fnn_mean_quant(2.0, 16U, &mean.mult, &mean.rsh) != FNN_OK ||
      fnn_mean_check(&mean) != FNN_OK) {
    return 1;
  }
  int8_t m;
  fnn_mean_s8(&mean, img, &m);
  printf("mean %d\n", m);
  return 0;
}
```

Which prints:

```text
max 6 8 -1 12
avg 4 6 -3 11
mean 9
```

The top-left window holds 1, 2, 5 and 6: its maximum is 6 and its average 3.5,
which rounds to 4. The bottom-left one averages to -2.5 and rounds to -3:
averages round half away from zero, as TFLite's do, in every rounding mode.
Pools keep the input's scale and zero point, so they take no multiplier, only
a clamp for a fused ReLU. Taps that fall in the padding are left out, so a
window on the border averages fewer values. A window the size of the input
pools globally.

The mean sums the 16 values to 68 and scales the sum by the input scale over
the output scale, here 2, divided by 16: 8.5, which rounds to 9.
`fnn_mean_quant` builds that multiplier the way TFLite does, truncating the
division by n, and the sum is requantised like any layer's, so in
`FNN_ROUND_GEMMLOWP` the result is TFLite's bit for bit. That is why the two
are separate: Keras's global average pooling converts to TFLite's MEAN, and
MEAN rounds differently from AVERAGE_POOL_2D even when the scales are equal.

Cycles on an STM32F103 at 72 MHz (`-O2`), against CMSIS-NN 8.0.0 built the same
way:

| layer | flexnn | CMSIS-NN |
| --- | --: | --: |
| max pool 2 x 2, stride 2, 16 x 16 x 16 | 44004 | 52027 |
| average pool 5 x 5 x 64 to 1 x 1 | 9362 | 30305 |
| mean of 5 x 5 x 64 | 9344 | - |

The max pool compares four channels at once, a byte each in one 32-bit word,
and the average sums four channels in registers. That is 10.7 cycles per input
value for the max pool and 5.9 for the average; most of it goes to the loops
around so few taps, not to the comparisons and sums themselves.

## Adding and joining tensors (`fnn_elem.h`, `fnn_shape.h`)

A residual block adds its input back to its output, and a network with two
branches joins them before the next layer. In int8 the add is the harder of
the two: each input has its own scale and zero point, and so does the sum.

```c
#include <stdio.h>

#include "fnn_elem.h"
#include "fnn_shape.h"

int main(void) {
  /* 1.0, 2.0, -3.0, 4.0 at a scale of 0.1 */
  static const int8_t a[4] = {10, 20, -30, 40};
  /* 3.0, 3.0, 3.0, -23.6 at a scale of 0.2 and a zero point of -10 */
  static const int8_t b[4] = {5, 5, 5, -128};
  FNN_Add add;
  if (fnn_add_init(&add, 0.1, 0, 0.2, -10, 0.25, (FNN_Out){0, -128, 127}) !=
      FNN_OK) {
    return 1;
  }
  int8_t sum[4];
  fnn_add_s8(&add, a, b, sum, 4U);
  printf("sum %d %d %d %d\n", sum[0], sum[1], sum[2], sum[3]);

  /* a and b as two pixels of two channels each, joined along the channels */
  const int8_t *const in[2] = {a, b};
  static const uint32_t len[2] = {2U, 2U};
  int8_t cat[8];
  fnn_concat_s8(in, len, 2U, 2U, cat);
  printf("cat");
  for (unsigned i = 0U; i < 8U; i++) {
    printf(" %d", cat[i]);
  }
  printf("\n");
  return 0;
}
```

Which prints:

```text
sum 16 20 0 -78
cat 10 20 5 5 -30 40 5 -128
```

The real sums are 4, 5, 0 and -19.6, which at an output scale of 0.25 are 16,
20, 0 and -78.4, rounded to -78. `fnn_add_init` turns the three scales into
multipliers once, at setup. In `FNN_ROUND_GEMMLOWP` it derives them the way
TFLite does, and the add is TFLite's bit for bit: each input is shifted up
20 bits, scaled to half the larger input scale and rounded, and the sum is
scaled to the output and rounded again. In the other modes both inputs are
scaled to the output at once and the exact sum is rounded a single time. The
output may be either input, so an add can run in place.

`fnn_concat_s8` sees each tensor as rows: for NHWC tensors joined along their
channels, a row is one pixel, so here each of the two rows takes two channels
from each input. TFLite requires the inputs of an int8 concatenation to share
the output's scale and zero point, so the values are copied unchanged.
Reshaping and flattening need no call at all: the tensor is the same bytes.

Cycles on an STM32F103 at 72 MHz (`-O2`), against CMSIS-NN 8.0.0 built the same
way, with equal scales:

| layer | flexnn | CMSIS-NN |
| --- | --: | --: |
| add, 1024 values | 30467 | 110158 |
| add, 1024 values, `FNN_ROUND_GEMMLOWP` | 90682 | 110158 |
| concatenation, 2 x 8 x 8 x 16 to 8 x 8 x 32 | 10102 | 17375 |

The add is 30 cycles a value in the default mode: two 64-bit multiply-adds
from a constant that already holds both zero points, half a step and the
output zero point. TFLite's three roundings take three times as long, still
about a fifth fewer cycles than CMSIS-NN, which does the same arithmetic.

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

## A model's inputs and outputs (`fnn_convert.h`)

Your sensor data arrives as q15, q31 or float, and a model's first layer takes
int8 at the scale and zero point the model was trained with. The quantise
functions convert it, and the dequantise functions turn the last layer's
outputs back into q15 or float:

```c
#include <stdio.h>

#include "fnn_convert.h"
#include "fnn_quant.h"

int main(void) {
  /* the model's input: a scale of 0.05 and a zero point of -3 */
  int32_t mult;
  int8_t rsh;
  if (fnn_quant_from_scale(1.0 / 0.05, &mult, &rsh) != FNN_OK) {
    return 1;
  }
  const float in[4] = {0.0f, 1.0f, -2.57f, 100.0f};
  int8_t q[4];
  fnn_quantize_f32_s8(in, q, 4U, mult, rsh, -3);
  printf("%d %d %d %d\n", q[0], q[1], q[2], q[3]);

  /* and back */
  float out[4];
  fnn_dequantize_s8_f32(q, out, 4U, 0.05f, -3);
  printf("%g %g %g %g\n", (double)out[0], (double)out[1], (double)out[2],
         (double)out[3]);
  return 0;
}
```

Which prints:

```text
-3 17 -54 127
0 1 -2.55 6.5
```

Zero lands on the zero point. 1.0 / 0.05 is 20, which with the zero point
gives 17, and -2.57 / 0.05 = -51.4 rounds to -51, which gives -54. 100 is far
past the int8 range and saturates at 127, which back in float can only stand
for 6.5.

The quantise functions take the scale as a multiplier from
`fnn_quant_from_scale`: 1 / s for float input, 2^-15 / s for q15 and
2^-31 / s for q31. Each value is then rounded once from its exact product with
that multiplier, the way `FNN_ROUNDING` says, with no float division. Infinities
saturate and a NaN gives -128. Dequantising to float gives exactly the float
product s (q - z), rounded to nearest, ties to even, and for ordinary scales
without a soft-float multiply. Dequantising to q15 takes the multiplier for 2^15 s.

TFLite's QUANTIZE from int16 rounds twice, as gemmlowp does: build with
`FNN_ROUND_GEMMLOWP` to match it bit for bit. From float, TFLite divides in
float32 and then rounds the quotient, a second rounding: a value such as
-13.4999995 becomes -13.5 and then -14, where flexnn gives -13. Otherwise
`FNN_ROUND_AWAY` matches it.

Cycles on an STM32F103 at 72 MHz (`-O2`), against TFLite's formulas compiled
for the Cortex-M3, where float arithmetic is software:

| conversion | values | flexnn | per value | TFLite's formula |
| --- | --: | --: | --: | --: |
| float to int8 | 128 | 5199 | 41 | 26154 |
| q15 to int8 | 256 | 5739 | 22 | - |
| q31 to int8 | 256 | 5971 | 23 | - |
| int8 to float | 128 | 3901 | 30 | 9694 |
| int8 to q15 | 256 | 6032 | 24 | - |

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
| `FNN_ROUND_AWAY` | once, ties away from zero | the TFLite (LiteRT) interpreter's reference fully connected layer |
| `FNN_ROUND_GEMMLOWP` | twice (gemmlowp) | CMSIS-NN's default build and TFLite Micro, bit for bit |

In the example only -384 changes: `FNN_ROUND_AWAY` and `FNN_ROUND_GEMMLOWP`
give -12. Rounding once from the exact product is the flexlib rule and the
default.

When the board has to give exactly the outputs a model gives under TFLite
Micro or CMSIS-NN, which is how TFLite models run on a Cortex-M today, build
with `FNN_ROUND_GEMMLOWP`. The fully connected, convolution, depthwise and
softmax layers are checked against CMSIS-NN compiled on the PC in that mode,
output for output.

The TFLite interpreter on your PC is a looser match, because its reference
kernels do not share one rule: its fully connected layer rounds like
`FNN_ROUND_AWAY`, and its convolution, depthwise convolution and QUANTIZE from
int16 like `FNN_ROUND_GEMMLOWP`. In any one mode a whole model can then differ
from it by 1 in a few outputs. Its optimised kernels, which it uses unless you
ask for the reference ones, round differently again (half to even, through
float).

Every input gives a defined result. Where TFLite's arithmetic would overflow
int32 (scales of 1/2 and above with large sums), flexnn saturates.

## How it was checked

Fully connected layers against a reference written in TFLite's own terms, with
the zero point subtracted per input, in both layouts and every rounding mode:
random layers of 0 to 256 inputs and 1 to 70 outputs, weights and inputs at
their extremes, per-tensor and per-channel scales, fused clamps, and a folded
bias that wraps past int32 while the true sum fits. Four layers, up to 256
inputs and 70 outputs, against the TFLite interpreter, bit for bit.

Against CMSIS-NN 8.0.0 compiled on the PC, in `FNN_ROUND_GEMMLOWP`: 400 random
fully connected layers, per tensor and per channel, and 600 random
convolutions through `arm_convolve_wrapper_s8`, which picks CMSIS-NN's 1 x 1,
1 x n and general paths, in both weight layouts, 600 depthwise layers through
`arm_depthwise_conv_wrapper_s8`, 2000 softmax rows at an input scale of
1/16, random max and average pools with padding and clamps, 124,139 and
105,036 outputs, and 1000 random adds, 2,071,493 outputs. A quarter of the layers use a power-of-two scale, which makes exact ties;
built with `FNN_ROUND_UP` instead, the same check finds hundreds of
differences.

Depthwise layers against a reference in TFLite's terms: 3000 random layers with
filters up to 5 x 5, channel multipliers 1 to 3, strides 1 and 2, VALID and
SAME padding and 1 to 64 channels, in every rounding mode, a quarter of them
at a scale that makes exact ties. Against the TFLite interpreter, four layers
bit for bit in `FNN_ROUND_GEMMLOWP`.

Convolutions against a reference written in TFLite's terms, which subtracts
the zero point per tap and skips the taps outside the input: 3000 random layers
with filters up to 5 x 5, strides 1 and 2, VALID and SAME padding, 1 to 16
input and 1 to 17 output channels, in both layouts and every rounding mode,
plus the bench's layers and a 10 x 4 filter. Against the TFLite interpreter,
five layers including a stride-2 SAME layer and a filter larger than its
input, bit for bit in `FNN_ROUND_GEMMLOWP`, the interpreter's rule for
convolution.

Pools and means against references in TFLite's terms: 3191 random pools of
1 to 64 channels with windows up to 5 x 5, strides 1 to 3, VALID and SAME
padding and random clamps, both kinds each time, and 2933 random means at
equal, power-of-two and other scales. Global pools and means up to 300 values
wide, with inputs all -128 and all 127. Against the TFLite interpreter, eight
pools bit for bit in every rounding mode, and five means bit for bit in
`FNN_ROUND_GEMMLOWP`.

Adds against a reference in TFLite's terms for `FNN_ROUND_GEMMLOWP` and an
exact 64-bit sum for the other modes: all 65,536 pairs of int8 inputs at nine
sets of scales and zero points, among them equal scales, scales that put sums
on exact ties, an output scale so large every sum rounds to 0, and fused
clamps; 200 random adds, in place over either input. Against the TFLite
interpreter, five adds bit for bit in `FNN_ROUND_GEMMLOWP`. Concatenation
against a plain copy at 500 random shapes and alignments, and against the
interpreter along the channels, height and width, bit for bit in every mode.

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

Conversions: every q15 input and every float bit pattern, at several scales and
in each rounding mode, against a model built on the requantisation reference.
Dequantising to float on every scale mantissa against every q - z, bit for bit
with the float multiply. Against the TFLite interpreter: QUANTIZE from int16
and float at five scales, and DEQUANTIZE at four, scales near the float limits
included.

Requantisation: every int32 sum, for 15 multiplier and shift pairs at the ends of their
ranges, in each rounding mode, against an exact 64-bit model. Random sums,
multipliers and every shift against a second model written in TFLite's and
gemmlowp's own terms. `fnn_quant_from_scale` against TFLite's
`QuantizeMultiplier` on 100,000 random scales and the edge cases (scales that
round up to the next power of two, scales too small or too large to
represent). [docs/testing.md](../docs/testing.md) lists the commands.
