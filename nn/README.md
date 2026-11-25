# flexnn

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexnn-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

flexnn runs quantised int8 neural networks on Cortex-M with the arithmetic of
TFLite's int8 models. Depending on the rounding mode the build selects, its
outputs match those of the TFLite interpreter, TFLite Micro or CMSIS-NN.

It provides fully connected, convolution and depthwise layers, pooling and
mean, add and concatenation, activation functions as lookup tables, softmax and
argmax, and conversions to and from int8. Pairs of layers can be fused into one
pass, 1-D layers can run one input column at a time, and convolution weights
can be stored as int4 or ternary. The library is written in C11 with no dynamic
allocation.

`nn/tools/fnn_tflite.py` converts a full-integer int8 `.tflite` model into C
that calls these layers. It requires Python 3.10 or later and does not require
TensorFlow.

```sh
pip install -r lib/flexlib/nn/tools/requirements.txt
python3 lib/flexlib/nn/tools/fnn_tflite.py mlp.tflite -o src/ --check
```

For a small Keras classifier with two hidden `Dense` layers and a softmax over
4 classes, the converter writes `src/mlp.c` and `src/mlp.h`. With `--check` it
also runs the generated C and the TFLite interpreter on the same 200 inputs and
reports the differences.

```text
mlp: 8 operators, 3 of them only shapes and 0 paddings. Arena 48 B, with 48 B live at the busiest layer. Constants 4116 B.
check (tflite): 0 of 800 outputs differ from the TFLite interpreter
```

The application calls `mlp_run` with the input, the output and an arena of
`MLP_ARENA_SIZE` bytes.

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

It prints

```text
class 0: 0.1445
class 1: 0.1992
class 2: 0.2773
class 3: 0.3789
```

These values match the TFLite interpreter's output for the same input. Weights,
biases and multipliers are emitted as `const` arrays in flash. The arena holds
the intermediate tensors, and tensors that are not live at the same time share
memory. Weights are emitted in flexnn's packed order, and the generated C
checks `FNN_LAYOUT_VERSION` at compile time. If the check fails after a flexnn
update, convert the model again.

Cycle counts below are for one call on an STM32F103 at 72 MHz (`-O2`), measured
against CMSIS-NN 8.0.0 built with the same flags. The full results are in
[docs/results/f103.md](../docs/results/f103.md).

## Models

The converter supports fully connected, convolution and depthwise layers, max
and average pooling, MEAN over height and width, ADD, concatenation, softmax,
and LOGISTIC, TANH, HARD_SWISH, LEAKY_RELU, RELU and RELU6 as tables. A PAD
before a convolution becomes the convolution's padding, and RESHAPE and the
shape arithmetic around it are resolved at conversion. Other operators are
rejected with an error that gives the operator's index and name.

| model | multiply-accumulates | arena | flexnn | CMSIS-NN |
| --- | --: | --: | --: | --: |
| R2, IMU gesture, three 1-D convolutions, pools, a mean, dense 8 | 261,440 | 2928 B | 1,445,698 | 1,769,539 |
| R5, 28 x 28 CNN, two 3 x 3 convolutions, pools, dense 10, softmax | 290,080 | 2872 B | 2,331,975 | 2,855,963 |
| R3, keyword spotting, a 10 x 4 convolution, four depthwise and pointwise pairs, mean, dense 12, softmax | 2,656,768 | 9280 B | 17,821,032 | |

R2 and R5 take about 18% fewer cycles than the same layers through CMSIS-NN.
R5 fuses its convolutions with the max pools that follow them, which reduces
its arena from 6284 to 2872 bytes. R3 fuses its four depthwise and pointwise
pairs and runs in 248 ms. There is no CMSIS-NN figure for R3. Without fusion
a single pair needs 16,000 bytes of activations, more than the F103 has free
next to the benchmark.

With `--stream`, the converter also generates a step function for causal 1-D
models, which computes the output columns completed by a new block of input.
For R2, a step of 4 samples takes 59,044 cycles, against 1,445,717 for the
full window, and the outputs are identical.

## Layers

The layers are C functions that take int8 tensors and a `const` layer
description, and can be called directly without the converter.

| header | layers | case | flexnn | CMSIS-NN |
| --- | --- | --- | --: | --: |
| `fnn_dense.h` | fully connected | 64 -> 12, weights in the cache | 4156 | 5492 |
| | | 256 -> 64, weights from flash | 131903 | 164891 |
| `fnn_conv.h` | convolution, 1-D and 2-D | 1 x 1, 64 -> 64 channels, 5 x 5 | 520209 | 591662 |
| | | 3 x 3, 8 -> 16 channels, 12 x 12 | 902863 | 1135991 |
| `fnn_dwconv.h` | depthwise convolution | 3 x 3, 64 channels, 5 x 5 | 257137 | 470046 |
| `fnn_fused.h` | depthwise and 1 x 1, convolution and max pool | | | |
| `fnn_pool.h` | max and average pool, mean | max pool 2 x 2, 16 x 16 x 16 | 44004 | 52027 |
| `fnn_elem.h` | add, concatenation | add, 1024 values | 30467 | 110158 |
| `fnn_lut.h` | int8 functions as lookup tables | | | |
| `fnn_softmax.h` | softmax, argmax | softmax of 12 | 602 | 8159 |
| `fnn_convert.h` | to and from q15, q31 and float | | | |
| `fnn_stream.h` | 1-D layers one column at a time | | | |
| `fnn_quant.h` | requantisation | | | |

Fully connected and convolution layers take 14 to 27% fewer cycles with weights
reordered by `fnn_dense_pack_s8`.

Convolution weights in [-7, 7] can be packed into 4 bits with
`fnn_conv_pack_s4`, and weights in {-1, 0, 1} into 2 bits with
`fnn_conv_pack_s2`. They produce the same outputs as the same weights stored as
int8. The convolutions in the table take 22 to 26% fewer cycles than packed
int8 in 4 bits, and 14 to 20% fewer in 2. The converter uses these formats when
all of a convolution's weights are in range.

## Rounding

Int8 inference engines differ in how they round requantised sums, and their
results can differ by 1 in some outputs. `FNN_ROUNDING` selects one rule for
the whole build.

| `FNN_ROUNDING` | rounding | matches |
| --- | --- | --- |
| `FNN_ROUND_UP` (default) | once, ties up | CMSIS-NN built with `CMSIS_NN_USE_SINGLE_ROUNDING` |
| `FNN_ROUND_AWAY` | once, ties away from zero | the TFLite interpreter's reference fully connected layer |
| `FNN_ROUND_GEMMLOWP` | twice (gemmlowp) | CMSIS-NN's default build and TFLite Micro, identical outputs |
| `FNN_ROUND_TFLITE` | fully connected layers as `FNN_ROUND_AWAY`, the rest as `FNN_ROUND_GEMMLOWP` | the TFLite interpreter's reference kernels, a whole model |

`FNN_ROUND_GEMMLOWP` reproduces TFLite Micro and CMSIS-NN, and
`FNN_ROUND_TFLITE` reproduces the TFLite interpreter on the PC. In these two
modes softmax also uses TFLite's fixed-point arithmetic. Where TFLite's
arithmetic would overflow int32, flexnn saturates.

## Use

flexnn is part of the default `FLEXLIB_LIBS`, and CMake projects link
`flexlib::nn`. The rounding mode is a single define for the whole build, set in
`CFLAGS` or in CMake's `FLEXLIB_DEFINES`.

```make
CFLAGS += -DFNN_ROUNDING=FNN_ROUND_TFLITE
```

The generated C depends on the flexnn sources and its own header.

## Testing

The layers are tested for identical outputs against the TFLite interpreter's
reference kernels in all rounding modes, and the fully connected,
convolution, depthwise, pooling, add and softmax layers also against CMSIS-NN
compiled for the host. The converter's arithmetic and memory plans are tested
as well. See [docs/testing.md](../docs/testing.md).