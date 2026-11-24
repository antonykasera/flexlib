# flexnn

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexnn-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

flexnn runs small quantised neural networks on Cortex-M: int8 inference with
the arithmetic of TFLite's int8 models. A network you train and quantise on a
PC gives the same integers on the board. It has fully connected, convolution
and depthwise layers, pooling and mean, add and concatenation, activation
functions as tables, softmax and argmax, conversions in and out of int8, and
the step every layer ends with, turning its int32 sums into int8 outputs. Layer
pairs run in one pass, and 1-D layers one input column at a time.
`fnn_tflite.py` turns a TFLite model into C that calls these layers, as
[Converting a model](#converting-a-model-nntoolsfnn_tflitepy) shows, and
[A whole model, by hand](#a-whole-model-by-hand) puts one together without it.

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
`const` array. If you store packed weights, check `FNN_LAYOUT_VERSION`, which
goes up whenever the order changes. Pass the rows, not the packed weights, to
`fnn_fold_bias_s8`.

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

Tensors are NHWC, and the filter keeps TFLite's `[c_out][kh][kw][c_in]` order.
That is a dense layer's rows of kh kw c_in inputs, which is why
`fnn_fold_bias_s8` takes it as it is, and why `fnn_dense_pack_s8` packs it for
the same faster kernel. Padded taps read the input zero point, and with the
folded bias they add exactly nothing. The kernel copies a window into the
scratch only where it overhangs the input or spans more than one run of memory:
a 1 x 1 filter, or a 1-row filter over a signal, reads the input in place.
`fnn_conv_check` tells you whether a layer's shapes, strides and padding fit
together.

Cycles on an STM32F103 at 72 MHz (`-O2`, weights in the flash cache), against
CMSIS-NN 8.0.0 built the same way, with a multiplier per output channel:

| layer | flexnn, packed | flexnn, rows | CMSIS-NN | int4 | ternary |
| --- | --: | --: | --: | --: | --: |
| 1 x 1, 64 -> 64 channels, 5 x 5 | 520209 | 649619 | 591662 | 389597 | 431253 |
| 3 x 3, 8 -> 16 channels, 12 x 12, SAME | 902863 | 1111252 | 1135991 | 702573 | 772177 |
| 5 taps, 16 -> 32 channels, 64 long | 802545 | 1007114 | 979578 | 592551 | 638521 |

Packed, that is 4.9 to 5.4 cycles per multiply-accumulate, 12 to 20% fewer
cycles than CMSIS-NN. In TFLite's order the filter takes from 2% fewer to 10%
more than CMSIS-NN: pack it when you can.

### Int4 and ternary weights

A model trained with its weights in [-7, 7], or in {-1, 0, 1}, can keep them in
4 or 2 bits. Pack each layer with `fnn_conv_pack_s4` or `fnn_conv_pack_s2`,
set `packed` to `FNN_PACK_S4` or `FNN_PACK_S2`, and give `fnn_conv_s8`
`FNN_CONV_SUB_SCRATCH()` bytes of 4-byte aligned scratch. The outputs are the
ones the same weights give as int8.

- int4 takes 22 to 26% fewer cycles than packed int8 in the table above, in half
  the flash. Ternary takes 14 to 20% fewer, in a quarter.
- `fnn_tflite.py` packs a convolution this way whenever all its weights fit, so
  an int8 `.tflite` from quantisation-aware training needs nothing more.

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
input channel c / m. Padded taps take the input zero point.

Cycles on an STM32F103 at 72 MHz (`-O2`), against CMSIS-NN 8.0.0 built the same
way, with a multiplier per channel:

| layer | flexnn | CMSIS-NN |
| --- | --: | --: |
| 3 x 3, 64 channels, 5 x 5, SAME | 257137 | 470046 |
| 3 x 3, 64 channels, 7 x 7 to 5 x 5, VALID | 230247 | - |

That is 45% fewer cycles than CMSIS-NN's faster depthwise kernel, but still 16
to 18 cycles per multiply-accumulate. Taps in the padding cost one product per
channel, the zero point times the sum of their weights. The loop over the other
taps is mostly 32-bit instructions, and flash at two wait states delivers under
three bytes of code a cycle. The loop runs at about that rate, not at the rate
of its multiplies.

## Depthwise and pointwise in one pass (`fnn_fused.h`)

`fnn_dwpw_s8` runs a depthwise layer and the 1 x 1 convolution after it a row
at a time, and the result can go over the input:

```c
#include <stdio.h>

#include "fnn_fused.h"
#include "fnn_quant.h"

int main(void) {
  /* 3 x 3 pixels of two channels: the first counts 1 to 9, the second is 1 */
  int8_t x[18];
  for (int i = 0; i < 9; i++) {
    x[2 * i] = (int8_t)(i + 1);
    x[2 * i + 1] = 1;
  }
  int8_t wd[18], wp[4] = {0, 1, 1, 0};
  for (int i = 0; i < 18; i++) {
    wd[i] = 1;
  }
  int32_t mult;
  int8_t rsh;
  fnn_quant_from_scale(1.0, &mult, &rsh);
  const FNN_DwConv dw = {wd, NULL, {3, 3, 2}, {3, 3, 2}, 3, 3, 1, 1, 1, 1, 0,
                         {&mult, &rsh, 1}, {0, -128, 127}};
  const FNN_Conv pw = {wp, NULL, {3, 3, 2}, {3, 3, 2}, 1, 1, 1, 1, 0, 0, 0, 0,
                       {&mult, &rsh, 1}, {0, -128, 127}};
  int8_t scratch[FNN_DWPW_SCRATCH(3, 3, 2, 2)];
  if (fnn_dwpw_check(&dw, &pw) != FNN_OK) {
    return 1;
  }
  fnn_dwpw_s8(&dw, &pw, x, x, scratch);
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      printf(" %d/%d", x[(r * 3 + c) * 2], x[(r * 3 + c) * 2 + 1]);
    }
    printf("\n");
  }
  return 0;
}
```

Which prints:

```text
 4/12 6/21 4/16
 6/27 9/45 6/33
 4/24 6/39 4/28
```

Besides `x`, the block used 24 bytes of scratch. A 1 x 1 layer with int4 or
ternary weights needs `FNN_DWPW_SUB_SCRATCH()` bytes instead.

For a 12 x 5 x 64 block, 3 x 3 then 64 to 64 channels, on an STM32F103 at
72 MHz (`-O2`):

| block | memory | cycles |
| --- | --: | --: |
| `fnn_dwconv_s8` then `fnn_conv_s8` | 7680 B | 1877523 |
| `fnn_dwpw_s8`, in place | 5120 B | 1912743 |

`fnn_tflite.py` fuses such a pair when that makes the arena smaller.

## A convolution and its max pool in one pass (`fnn_fused.h`)

`fnn_convpool_s8` runs a convolution and the max pool after it a pooled row at
a time, without the convolution's whole output:

```c
#include <stdio.h>

#include "fnn_fused.h"
#include "fnn_quant.h"

int main(void) {
  /* a 4 x 4 image of one channel, counting 1 to 16 */
  int8_t x[16], w[9], y[4];
  for (int i = 0; i < 16; i++) {
    x[i] = (int8_t)(i + 1);
  }
  for (int i = 0; i < 9; i++) {
    w[i] = 1;
  }
  int32_t mult;
  int8_t rsh;
  fnn_quant_from_scale(1.0, &mult, &rsh);
  const FNN_Conv c = {w, NULL, {4, 4, 1}, {4, 4, 1}, 3, 3, 1, 1, 1, 1, 0, 0,
                      {&mult, &rsh, 1}, {0, -128, 127}};
  const FNN_Pool p = {{4, 4, 1}, {2, 2, 1}, 2, 2, 2, 2, 0, 0, -128, 127};
  int8_t scratch[FNN_CONVPOOL_SCRATCH(2, 4, 1, 3, 3, 1)];
  if (fnn_convpool_check(&c, &p) != FNN_OK) {
    return 1;
  }
  fnn_convpool_s8(&c, &p, x, y, scratch);
  printf("%d %d\n%d %d\n", y[0], y[1], y[2], y[3]);
  return 0;
}
```

Which prints:

```text
54 63
90 99
```

The scratch held two convolution rows and one patch, 17 bytes.

For R5's second pair, a 3 x 3 convolution from 8 to 16 channels on 14 x 14 and
a 2 x 2 max pool, on an STM32F103 at 72 MHz (`-O2`):

| block | memory | cycles |
| --- | --: | --: |
| `fnn_conv_s8` then `fnn_maxpool_s8` | 5560 B | 1250333 |
| `fnn_convpool_s8` | 2872 B | 1252292 |

`fnn_tflite.py` fuses such a pair when that makes the arena smaller.

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
averages round half away from zero in every rounding mode. Pools keep the
input's scale and zero point. They take no multiplier, only a clamp for a fused
ReLU. Taps that fall in the padding are left out, and a window on the border
averages fewer values. A window the size of the input pools globally. A pool
can also write its output over its input, as long as no padding sits above or
to the left of the input. Each window then starts at or after the output it
writes, and you save the output's buffer.

The mean sums the 16 values to 68 and scales the sum by the input scale over
the output scale, here 2, divided by 16: 8.5, which rounds to 9.
`fnn_mean_quant` builds that multiplier and truncates the division by n. The
sum is then requantised like any layer's, and in `FNN_ROUND_GEMMLOWP` the
result is TFLite's MEAN bit for bit. That is why the two are separate: Keras's
global average pooling converts to TFLite's MEAN, and MEAN rounds differently
from AVERAGE_POOL_2D even when the scales are equal.

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
multipliers once, at setup. In `FNN_ROUND_GEMMLOWP` the add is TFLite's ADD bit
for bit: each input is shifted up 20 bits, scaled to half the larger input
scale and rounded, and the sum is scaled to the output and rounded again. In
the other modes both inputs are scaled to the output at once and the exact sum
is rounded a single time. The output may be either input, which lets an add run
in place.

`fnn_concat_s8` sees each tensor as rows. For NHWC tensors joined along their
channels, a row is one pixel, and here each of the two rows takes two channels
from each input. TFLite requires the inputs of an int8 concatenation to share
the output's scale and zero point. The values are copied unchanged. Reshaping
and flattening need no call at all: the tensor is the same bytes.

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

The inputs stand for -2, -0.5, 0, 0.5 and 2. Their tanh, -0.964 to 0.964, is
-123.4 to 123.4 in steps of 1/128, and each rounds to the nearest integer.
`fnn_lut_s8_build` takes any `double` function of one `double`: `tanh` from
math.h, or one of your own for the logistic function, hard-swish or leaky ReLU.
It works in double precision, which a Cortex-M3 runs in software: build tables
at startup or ahead of time, not per inference. The output can overwrite the
input, and here it does.

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
scores it picks the first.

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

TFLite's QUANTIZE from int16 rounds twice. Build with `FNN_ROUND_GEMMLOWP` or
`FNN_ROUND_TFLITE` to match it bit for bit. From float, TFLite divides in
float32 and then rounds the quotient, a second rounding: a value such as
-13.4999995 becomes -13.5 and then -14, where flexnn gives -13. Otherwise
`FNN_ROUND_AWAY`, `FNN_ROUND_GEMMLOWP` and `FNN_ROUND_TFLITE` match it.

Cycles on an STM32F103 at 72 MHz (`-O2`), against TFLite's formulas compiled
for the Cortex-M3, where float arithmetic is software:

| conversion | values | flexnn | per value | TFLite's formula |
| --- | --: | --: | --: | --: |
| float to int8 | 128 | 5199 | 41 | 26154 |
| q15 to int8 | 256 | 5739 | 22 | - |
| q31 to int8 | 256 | 5971 | 23 | - |
| int8 to float | 128 | 3901 | 30 | 9694 |
| int8 to q15 | 256 | 6032 | 24 | - |

## A whole model, by hand

Layers chain through their buffers: each layer writes the next one's input,
and the next layer takes that tensor's zero point as its `zx`. Here is the
smallest network that needs a hidden layer, an XOR made of two ReLU units:

```c
#include <stdio.h>

#include "fnn_dense.h"
#include "fnn_quant.h"

int main(void) {
  /* XOR from two ReLU units: h0 = relu(a + b), h1 = relu(a + b - 1),
   * and y = h0 - 2 h1. Every scale is 1 and every zero point 0. */
  static const int8_t w1[2 * 2] = {1, 1, 1, 1}, w2[2] = {1, -2};
  static const int32_t b1[2] = {0, -1};
  int32_t bf1[2], bf2[1], mult;
  int8_t rsh;
  fnn_fold_bias_s8(w1, 2U, 2U, b1, 0, bf1);
  fnn_fold_bias_s8(w2, 2U, 1U, NULL, 0, bf2);
  if (fnn_quant_from_scale(1.0, &mult, &rsh) != FNN_OK) {
    return 1;
  }
  /* the ReLU is the clamp: nothing below the zero point */
  const FNN_Dense hidden = {w1, bf1, 2U, 2U, 0U, {&mult, &rsh, 1U}, {0, 0, 127}};
  const FNN_Dense out = {w2, bf2, 2U, 1U, 0U, {&mult, &rsh, 1U}, {0, -128, 127}};
  for (int a = 0; a < 2; a++) {
    for (int b = 0; b < 2; b++) {
      const int8_t x[2] = {(int8_t)a, (int8_t)b};
      int8_t h[2], y;
      fnn_dense_s8(&hidden, x, h);
      fnn_dense_s8(&out, h, &y);
      printf("%d xor %d = %d\n", a, b, y);
    }
  }
  return 0;
}
```

Which prints:

```text
0 xor 0 = 0
0 xor 1 = 1
1 xor 0 = 1
1 xor 1 = 0
```

With both inputs at 1, the hidden layer gives 2 and 1, and the output layer 2 -
2 x 1 = 0. A fused ReLU is only the clamp: the hidden layer's `lo` is its zero
point, and nothing below 0 survives. For a model you trained, each layer's
weights, biases, scales and zero points come from its `.tflite` file, and its
multiplier from `fnn_quant_from_scale` of the input scale times the weight
scale over the output scale.

Three such models, built in TFLite's format with random weights, run end to
end in flexnn exactly as in the TFLite interpreter. On an STM32F103 at 72 MHz
(`-O2`), with packed weights, against the same layers through CMSIS-NN 8.0.0:

| model | multiply-accumulates | flexnn | CMSIS-NN |
| --- | --: | --: | --: |
| R0, XOR MLP 2 -> 4 -> 1 | 12 | 631 | 974 |
| R1, MLP 64 -> 32 -> 16 -> 4, softmax | 2624 | 14492 | 22844 |
| R4, autoencoder 64 -> 32 -> 8 -> 32 -> 64 | 4608 | 27585 | 36684 |

That is 1.3 to 1.6 times as fast as CMSIS-NN, and R1 takes 0.2 ms.

## Converting a model (`nn/tools/fnn_tflite.py`)

Writing the layers out by hand stops being practical past a few of them. The
converter reads the model TensorFlow gives you, a full-integer int8 `.tflite`
file, and writes the C: every weight as a constant array, one flexnn layer
per operator, and a function that runs them in order. It needs Python 3.10 or
later, `flatbuffers`, `numpy` and the `tflite` schema bindings, and no
TensorFlow; `--check` also needs `ai-edge-litert`. The requirements file has
all four:

```sh
pip install -r lib/flexlib/nn/tools/requirements.txt
python3 lib/flexlib/nn/tools/fnn_tflite.py mlp.tflite -o src/ --check
```

For a small Keras classifier, a `Flatten` of 4 x 16 features, two hidden
`Dense` layers with ReLU and a softmax over 4 classes, it prints:

```text
mlp: 8 operators, 3 of them only shapes and 0 paddings. Arena 48 B, with 48 B live at the busiest layer. Constants 4116 B.
   op  operator           kernel         MACs   flash  reads        writes           scratch
    4  FULLY_CONNECTED    dense          2048  2336 B  in           0
    5  FULLY_CONNECTED    dense           512   656 B  0            32
    6  FULLY_CONNECTED    dense            64   100 B  32           0
    7  SOFTMAX            softmax           0  1024 B  0            out
check (tflite): 0 of 800 outputs differ from the TFLite interpreter
```

and writes `src/mlp.c` and this `src/mlp.h`:

```c
/* generated by fnn_tflite from mlp.tflite: do not edit */
#ifndef MLP_H
#define MLP_H

#include <stdint.h>

#define MLP_IN_LEN 64u
#define MLP_OUT_LEN 4u
/* Bytes of working memory the model needs, 4-byte aligned. */
#define MLP_ARENA_SIZE 48u
#define MLP_IN_SCALE 0.029791297f
#define MLP_IN_ZERO_POINT (1)
#define MLP_OUT_SCALE 0.00390625f
#define MLP_OUT_ZERO_POINT (-128)

/* Runs the model on the MLP_IN_LEN values at in and writes MLP_OUT_LEN
 * values to out, using the MLP_ARENA_SIZE bytes at arena. */
void mlp_run(const int8_t *in, int8_t *out, uint8_t *arena);

#endif /* MLP_H */
```

Your code gives `mlp_run` the input, the output and an arena of
`MLP_ARENA_SIZE` bytes, and reads the scale and zero point from the header:

```c
#include <stdio.h>

#include "mlp.h"

int main(void) {
  static uint32_t arena[(MLP_ARENA_SIZE + 3u) / 4u]; /* 4-byte aligned */
  int8_t in[MLP_IN_LEN], out[MLP_OUT_LEN];
  /* four rows of a ramp from -60 to 60 */
  for (unsigned i = 0U; i < MLP_IN_LEN; i++) {
    in[i] = (int8_t)((int)(i % 16U) * 8 - 60);
  }
  mlp_run(in, out, (uint8_t *)arena);
  for (unsigned i = 0U; i < MLP_OUT_LEN; i++) {
    printf("class %u: %.4f\n", i,
           (double)((float)(out[i] - MLP_OUT_ZERO_POINT) * MLP_OUT_SCALE));
  }
  return 0;
}
```

Which prints:

```text
class 0: 0.1445
class 1: 0.1992
class 2: 0.2773
class 3: 0.3789
```

The weights here are random and the classes mean nothing. The four numbers are
what the TFLite interpreter gives for the same input. TensorFlow writes
`Flatten` as five operators: SHAPE, STRIDED_SLICE and PACK compute the shape,
and RESHAPE applies it. Every shape is known before the model runs, and the
converter works the first three out itself and turns the RESHAPE into nothing:
the first layer reads the input where it is. The arena holds only the tensors
between layers, and two tensors share bytes when no layer needs both, which is
why three hidden tensors fit in 48 bytes. The input and output stay in your
buffers. The constants go to flash: the weights, the biases with the input zero
point folded in, a multiplier per output channel, and the softmax's table of
exponentials, written out so that the board never calls `exp`. Fully connected
and convolution weights are written in the packed order the faster kernels
read. That order belongs to the flexnn release, and the generated C checks
`FNN_LAYOUT_VERSION` when it compiles. If the check fails after you update
flexnn, convert the model again.

Each row of the report shows where a layer reads and writes, as arena offsets
or your `in` and `out`, and where its scratch goes. Pools with no padding above
or to the left, tables and adds can write their output over an input that no
later layer reads, and their rows say so. Here is the start of the report for
the small CNN in `test/nn/models/cnn_keras.tflite`:

```text
cnn_keras: 10 operators, 0 of them only shapes and 1 padding. Arena 1280 B, with 1280 B live at the busiest layer. Constants 3013 B. 2 of 2 layer pairs fused.
   op  operator           kernel         MACs   flash  reads        writes           scratch
  0+1  CONV_2D            convpool      55296   288 B  in           0                512
  2+3  DEPTHWISE_CONV_2D  dwpw           8704   280 B  0            512              1024
    4  ADD                add               0     0 B  0, 512       0, in place
```

Each fused pair shows both operators in one row. Run apart, these layers need
2076 bytes. A concatenation of single rows needs no copy, and its row says "no
copy". The first line gives the most bytes live at any one layer.

`--check` builds the generated C with flexnn on your PC, runs it and the TFLite
interpreter on 200 random inputs, or on the inputs in a `.npy` file given with
`--inputs`, and counts the outputs that differ. It needs `ai-edge-litert` and a
C compiler. It builds in `FNN_ROUND_TFLITE` by default, the interpreter's rule
for each operator. `--rounding gemmlowp` gives TFLite Micro's numbers instead,
and some of those differ from the interpreter's. Build your firmware in the
mode whose results you want.

The converter takes fully connected, convolution and depthwise layers, max and
average pooling, MEAN over height and width, ADD of two tensors of one shape,
concatenation, softmax, and LOGISTIC, TANH, HARD_SWISH, LEAKY_RELU, RELU and
RELU6 as tables. A PAD in front of a convolution becomes the convolution's own
padding, a RESHAPE costs nothing, and the shape arithmetic exporters write
around them is worked out at conversion. Anything else stops it with the
operator's index and name, as does a float input or output: convert with
`inference_input_type` and `inference_output_type` set to `tf.int8`.

The tables are the exact function, rounded once. For HARD_SWISH and
LEAKY_RELU that differs from the interpreter's own kernel by 1 on a few
inputs; `--tflite-tables` reads the interpreter's table instead, which needs
`ai-edge-litert` at conversion.

Here is how three converted models run on an STM32F103 at 72 MHz (`-O2`),
against the same layers through CMSIS-NN 8.0.0, with its convolution wrapper
picking each layer's kernel. The times are cycles for one inference, with the
weights in flash:

| model | multiply-accumulates | arena | flexnn | CMSIS-NN |
| --- | --: | --: | --: | --: |
| R2, IMU gesture, three 1-D convolutions, pools, a mean, dense 8 | 261,440 | 2928 B | 1,445,698 | 1,769,539 |
| R5, 28 x 28 CNN, two 3 x 3 convolutions, pools, dense 10, softmax | 290,080 | 2872 B | 2,331,975 | 2,855,963 |
| R3, keyword spotting, a 10 x 4 convolution, four depthwise and pointwise pairs, mean, dense 12, softmax | 2,656,768 | 9280 B | 17,821,032 | |

R2 and R5 run about 1.2 times as fast as CMSIS-NN: R2 in 20 ms and R5 in
32 ms. R5 runs each convolution with its max pool fused, in 2872 bytes where
the layers apart need 6284. CMSIS-NN has no MEAN, and its R2 uses a global
average pool in its place. R3 takes 248 ms with its four depthwise and pointwise pairs fused.
Through CMSIS-NN's layers it would need 16,000 bytes for one pair's depthwise
output and result alone, more than the F103 can spare next to the bench, and it
has no CMSIS-NN time here.

## Running a 1-D model as its input arrives (`fnn_stream.h`)

A causal 1-D layer can compute only the output column each new input column
completes, from a history of its last columns:

```c
#include <stdio.h>

#include "fnn_quant.h"
#include "fnn_stream.h"

int main(void) {
  /* a sum over the last 3 values of one channel */
  int8_t w[3] = {1, 1, 1}, hist[6], y;
  int32_t mult;
  int8_t rsh;
  fnn_quant_from_scale(1.0, &mult, &rsh);
  const FNN_Conv l = {w, NULL, {1, 8, 1}, {1, 6, 1}, 1, 3, 1, 1, 0, 0, 0, 0,
                      {&mult, &rsh, 1}, {0, -128, 127}};
  FNN_Stream s;
  if (fnn_stream_init(&s, hist, 6, 3, 1, 1) != FNN_OK ||
      fnn_stream_conv1d_check(&l, &s) != FNN_OK) {
    return 1;
  }
  for (int t = 1; t <= 8; t++) {
    int8_t x = (int8_t)(t * t);
    if (fnn_stream_conv1d_s8(&l, &s, &x, &y)) {
      printf("after %d: %d\n", t, y);
    }
  }
  return 0;
}
```

Which prints:

```text
after 3: 14
after 4: 29
after 5: 50
after 6: 77
after 7: 110
after 8: 127
```

Each line sums the last three squares, the last one clamped. `fnn_stream_push`
gives pools, means and your own layers the same windows.

With `--stream`, `fnn_tflite.py` also writes a step function that runs a 1-D
model as its input arrives. For the gesture model R2:

```text
r2: 7 operators, 0 of them only shapes and 0 paddings. Arena 2928 B, with 2928 B live at the busiest layer. Constants 6920 B. 0 of 2 layer pairs fused. Streams 6 layers: an output every 4 columns, 2302 B of history, step arena 32 B.
...
check (tflite): 0 of 1600 outputs differ from the TFLite interpreter
check (tflite): 0 of 800 streamed outputs differ from the batch run
```

The header gains:

```c
#define R2_STEP_IN_LEN 3u
#define R2_STEP_HOP 4u
#define R2_STEP_ARENA_SIZE 32u

typedef struct {
  FNN_Stream s[6];
  int8_t h[2302];
} R2_Stream;

void r2_stream_init(R2_Stream *st);
uint32_t r2_step(R2_Stream *st, const int8_t *in, int8_t *out,
                 uint8_t *arena);
```

Call `r2_stream_init` once, then `r2_step` with each new sample. Every 4
samples it returns 1, with the output for the 128 samples just ended. The
converter refuses SAME padding, more than one row and windows that leave
trailing columns, and names the layer.

On an STM32F103 at 72 MHz (`-O2`), for the same 128-sample windows:

| R2 | multiply-accumulates | memory | cycles |
| --- | --: | --: | --: |
| `r2_run` on the whole window | 261,440 | 2928 B | 1,445,717 |
| `r2_step` on 4 new samples | 9,408 | 2302 + 32 B | 59,044 |

Both give the same results.

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
shift. A layer you build by hand gets the numbers a converted model holds.
384 / 256 is 1.5 and rounds to 2, which the zero point moves to -8.
-384 / 256 is -1.5, exactly between -2 and -1: flexnn's default rounds ties
up, to -1, and the zero point makes it -11. -40000 / 256 is far below the
int8 range and clamps to -128. A per-channel layer passes one multiplier per
channel and sets `q.n` to the channel count. The clamp is also where a fused
ReLU or ReLU6 goes: it raises `lo` to the zero point, or lowers `hi`.

## Rounding

Three integer arithmetics are in use for int8 networks, and they differ by 1
in some outputs of a layer. You pick one for the whole build with
`FNN_ROUNDING`:

| `FNN_ROUNDING` | rounding | matches |
| --- | --- | --- |
| `FNN_ROUND_UP` (default) | once, ties up | CMSIS-NN built with `CMSIS_NN_USE_SINGLE_ROUNDING` |
| `FNN_ROUND_AWAY` | once, ties away from zero | the TFLite (LiteRT) interpreter's reference fully connected layer |
| `FNN_ROUND_GEMMLOWP` | twice (gemmlowp) | CMSIS-NN's default build and TFLite Micro, bit for bit |
| `FNN_ROUND_TFLITE` | fully connected layers as `FNN_ROUND_AWAY`, the rest as `FNN_ROUND_GEMMLOWP` | the TFLite interpreter's reference kernels, a whole model |

In the example only -384 changes: `FNN_ROUND_AWAY` and `FNN_ROUND_GEMMLOWP`
give -12. Rounding once from the exact product is the flexlib rule and the
default.

When the board has to give exactly the outputs a model gives under TFLite
Micro or CMSIS-NN, which is how TFLite models run on a Cortex-M today, build
with `FNN_ROUND_GEMMLOWP`. The fully connected, convolution, depthwise and
softmax layers are checked against CMSIS-NN compiled on the PC in that mode,
output for output.

The TFLite interpreter on your PC does not share one rule across its reference
kernels: its fully connected layer rounds like `FNN_ROUND_AWAY`, and its
convolution, depthwise convolution, mean, add and QUANTIZE like
`FNN_ROUND_GEMMLOWP`. `FNN_ROUND_TFLITE` follows it layer by layer, and a whole
model then gives the interpreter's integers. Build with it when the numbers you
trust are the ones you checked on the PC. Two limits remain. The interpreter's
fully connected layer rounds its result in double precision. The converter
multiplies every layer's scales in double too. A sum then has to fall within
about 2^-32 of a tie to round differently, and none did in 171 million.
QUANTIZE from float divides in float and rounds twice, and a few inputs next to
a tie can differ by 1. The interpreter's optimised kernels, which it uses
unless you ask for the reference ones, round differently again (half to even,
through float).

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

Depthwise and pointwise blocks, and convolutions with their max pools, against
the two layers run apart: 3000 random pairs of each in every rounding mode and
both weight layouts, in place where allowed. Streamed convolutions against the
batch layer on 3000 random 1-D layers, and streamed models against their batch
run on every window they end: R2, a Conv1D model as Keras exports it and twelve
random 1-D models.

Convolutions against a reference written in TFLite's terms, which subtracts
the zero point per tap and skips the taps outside the input: 3000 random layers
with filters up to 5 x 5, strides 1 and 2, VALID and SAME padding, 1 to 16
input and 1 to 17 output channels, in both layouts and every rounding mode,
plus the bench's layers and a 10 x 4 filter. Against the TFLite interpreter,
five layers including a stride-2 SAME layer and a filter larger than its
input, bit for bit in `FNN_ROUND_GEMMLOWP`, the interpreter's rule for
convolution.

Int4 and ternary convolutions against the same layers with int8 weights: 3000
random layers of each, rows of up to 600 weights, odd and even pixel counts,
and weights and inputs at their extremes. Depthwise and pointwise blocks with
int4 and ternary 1 x 1 layers, the scratch at every alignment. Against the
TFLite interpreter, R3 with int4 and with ternary convolutions, bit for bit in
`FNN_ROUND_TFLITE`.

Pools and means against references in TFLite's terms: 3191 random pools of 1 to
64 channels with windows up to 5 x 5, strides 1 to 3, VALID and SAME padding
and random clamps, both kinds each time. The 1677 with no padding above or to
the left run again in place, over a copy of the input. 2933 random means at
equal, power-of-two and other scales. Global pools and means up to 300 values
wide, with inputs all -128 and all 127. Against the TFLite interpreter, eight
pools bit for bit in every rounding mode, and five means bit for bit in
`FNN_ROUND_GEMMLOWP`.

Whole models: R0 (2 -> 4 -> 1), R1 (64 -> 32 -> 16 -> 4 with a softmax) and R4
(64 -> 32 -> 8 -> 32 -> 64), written as TFLite models with random weights and
ReLU on the hidden layers, against the interpreter on 500 random inputs each:
every output bit for bit in `FNN_ROUND_AWAY`, the interpreter's rule for
fully connected layers, and in `FNN_ROUND_TFLITE`, with each layer taking the
previous one's output.

The converter against the TFLite interpreter: R0, R1 and R4 written as models
and converted; R2 (three 1-D convolutions, pools, a mean and a dense layer), R5
(a 28 x 28 CNN) and R3 (keyword spotting, 2.66 million MACs); a model with
every operator, a PAD, a residual ADD, a concatenation and four kinds of table
among them; a model that joins heads along a row, one join inside another; a
model with a RESHAPE before and after its layer; and two models exported by
TensorFlow 2.21's own converter, a Keras MLP and a CNN with a ZeroPadding2D, a
residual add and GlobalAveragePooling2D. Every output bit for bit in
`FNN_ROUND_TFLITE`. Three fully connected layers at scales where the
interpreter's double-precision rounding and a multiplier from the float32
product part, every sum from -16384 to 16383, bit for bit in `FNN_ROUND_AWAY`
and `FNN_ROUND_TFLITE`. The converter's multipliers, softmax and activation
tables, clamps, mean and add setup, and packed weights against flexnn's own
functions on 68,583 values, and eight models it must refuse, each with the
message that says why. Every memory plan is replayed byte by byte at
conversion, each byte tagged by the value it holds, and the converter stops if
a layer would read an overwritten value. nn-check also pins the arena of each
model above and converts 300 random graphs of convolutions, depthwise and
pointwise pairs, pools, tables, adds and joined rows, ten of them against the
interpreter as well. Twenty-three small blocks check which layer pairs the
converter fuses, each also against the interpreter, and seven models check that
it refuses to stream what it cannot.

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
