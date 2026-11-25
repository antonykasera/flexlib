# flexnn

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/tag/antonykasera/flexlib?filter=flexnn-v%2A&sort=semver&label=release)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

int8 neural-network inference for Cortex-M with the arithmetic of TFLite's
int8 models. Depending on the rounding mode, outputs match the TFLite
interpreter, TFLite Micro or CMSIS-NN. It provides fully connected,
convolution and depthwise layers, pooling, mean, add, concatenation, lookup
table activations, softmax, argmax and int8 conversions, with fused layer
pairs, streaming 1-D layers and int4 or ternary convolution weights. Written in
C11 with no dynamic allocation.

## Converter

`nn/tools/fnn_tflite.py` converts a full-integer int8 `.tflite` model into C.
It requires Python 3.10 or later and does not require TensorFlow.

```sh
pip install -r lib/flexlib/nn/tools/requirements.txt
python3 lib/flexlib/nn/tools/fnn_tflite.py mlp.tflite -o src/ --check
```

```text
mlp: 8 operators, 3 of them only shapes and 0 paddings. Arena 48 B, with 48 B live at the busiest layer. Constants 4116 B.
check (tflite): 0 of 800 outputs differ from the TFLite interpreter
```

The converter writes `mlp.c` and `mlp.h`. `--check` compiles the generated C
on the host and compares it with the TFLite interpreter on 200 inputs.

Supported operators are fully connected, convolution, depthwise convolution,
max and average pooling, MEAN over height and width, ADD, concatenation,
softmax, PAD before a convolution, RESHAPE, and LOGISTIC, TANH, HARD_SWISH,
LEAKY_RELU, RELU and RELU6 as tables. Other operators are rejected with an
error that names the operator.

Weights, biases and multipliers are emitted as `const` arrays. The arena holds
the intermediate tensors, and tensors with disjoint lifetimes share memory.
Weights are emitted in the packed order of the flexnn release, and the
generated C checks `FNN_LAYOUT_VERSION` at compile time.

## Example

```c
#include <stdio.h>

#include "mlp.h"

int main(void) {
  static uint32_t arena[(MLP_ARENA_SIZE + 3u) / 4u]; /* 4-byte aligned */
  int8_t in[MLP_IN_LEN], out[MLP_OUT_LEN];
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

Output, identical to the TFLite interpreter for the same input:

```text
class 0: 0.1445
class 1: 0.1992
class 2: 0.2773
class 3: 0.3789
```

## Models

| Model | MACs | Arena | Cycles | CMSIS-NN |
| --- | --: | --: | --: | --: |
| R2, IMU gesture, three 1-D convolutions, pools, mean, dense 8 | 261,440 | 2928 B | 1,445,698 | 1,769,539 |
| R5, 28 x 28 CNN, two 3 x 3 convolutions, pools, dense 10, softmax | 290,080 | 2872 B | 2,331,975 | 2,855,963 |
| R3, keyword spotting, 10 x 4 convolution, four depthwise and pointwise pairs, mean, dense 12, softmax | 2,656,768 | 9280 B | 17,821,032 | \* |

One inference on an STM32F103 at 72 MHz (`-O2`), weights in flash, against
CMSIS-NN 8.0.0 built with the same flags. Full results are in
[docs/results/f103.md](../docs/results/f103.md).

\* Without fusion, one depthwise and pointwise pair of R3 needs 16,000 bytes of
activations, which does not fit next to the benchmark firmware.

R5 fuses its convolutions with the following max pools, which reduces its arena
from 6284 to 2872 bytes. With `--stream`, the converter also generates a step
function for causal 1-D models. For R2, a step of 4 samples takes 59,044
cycles, against 1,445,717 for the full window, with identical outputs.

## Layers

| Header | Layers | Case | Cycles | CMSIS-NN |
| --- | --- | --- | --: | --: |
| `fnn_dense.h` | Fully connected | 64 -> 12, weights in the cache | 4156 | 5492 |
| | | 256 -> 64, weights from flash | 131903 | 164891 |
| `fnn_conv.h` | Convolution, 1-D and 2-D | 1 x 1, 64 -> 64 channels, 5 x 5 | 520209 | 591662 |
| | | 3 x 3, 8 -> 16 channels, 12 x 12 | 902863 | 1135991 |
| `fnn_dwconv.h` | Depthwise convolution | 3 x 3, 64 channels, 5 x 5 | 257137 | 470046 |
| `fnn_fused.h` | Depthwise and 1 x 1, convolution and max pool | | | |
| `fnn_pool.h` | Max and average pool, mean | max pool 2 x 2, 16 x 16 x 16 | 44004 | 52027 |
| `fnn_elem.h` | Add, concatenation | add, 1024 values | 30467 | 110158 |
| `fnn_lut.h` | int8 functions as lookup tables | | | |
| `fnn_softmax.h` | Softmax, argmax | softmax of 12 | 602 | 8159 |
| `fnn_convert.h` | Conversion to and from q15, q31 and float | | | |
| `fnn_stream.h` | 1-D layers, one column at a time | | | |
| `fnn_quant.h` | Requantisation | | | |

The layers are C functions that take int8 tensors and a `const` layer
description, and can be used without the converter. Weights reordered by
`fnn_dense_pack_s8` reduce fully connected and convolution cycles by 14 to 27%.

## Sub-byte weights

| Format | Weights | Pack function | Cycles against packed int8 | Flash |
| --- | --- | --- | --: | --: |
| `FNN_PACK_S4` | [-7, 7] | `fnn_conv_pack_s4` | -22 to -26% | 1/2 |
| `FNN_PACK_S2` | {-1, 0, 1} | `fnn_conv_pack_s2` | -14 to -20% | 1/4 |

Outputs equal those of the same weights stored as int8. The converter selects
these formats when all weights of a convolution are in range.

## Rounding

| `FNN_ROUNDING` | Rounding | Matches |
| --- | --- | --- |
| `FNN_ROUND_UP` (default) | Once, ties up | CMSIS-NN built with `CMSIS_NN_USE_SINGLE_ROUNDING` |
| `FNN_ROUND_AWAY` | Once, ties away from zero | The TFLite interpreter's reference fully connected layer |
| `FNN_ROUND_GEMMLOWP` | Twice (gemmlowp) | CMSIS-NN's default build and TFLite Micro |
| `FNN_ROUND_TFLITE` | Fully connected as `FNN_ROUND_AWAY`, other layers as `FNN_ROUND_GEMMLOWP` | The TFLite interpreter's reference kernels |

The mode applies to the whole build. In `FNN_ROUND_GEMMLOWP` and
`FNN_ROUND_TFLITE`, softmax uses TFLite's fixed-point arithmetic. Results that
would overflow int32 in TFLite's arithmetic saturate.

## Usage

flexnn is part of the default `FLEXLIB_LIBS`, and CMake projects link
`flexlib::nn`. Set the rounding mode with a define:

```make
CFLAGS += -DFNN_ROUNDING=FNN_ROUND_TFLITE
```

In CMake, add it to `FLEXLIB_DEFINES`. Build integration is described in the
[root README](../README.md#usage).

## Testing

Layers are tested for identical outputs against the TFLite interpreter's
reference kernels in all rounding modes, and against CMSIS-NN compiled for the
host for the fully connected, convolution, depthwise, pooling, add and softmax
layers. The converter's arithmetic and memory plans are also tested. See
[docs/testing.md](../docs/testing.md).
