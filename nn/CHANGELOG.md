# flexnn changelog

## Unreleased

- `fnn_softmax_s8` and `fnn_softmax_s8_build` use TFLite's fixed-point softmax when built with `FNN_ROUND_GEMMLOWP` or `FNN_ROUND_TFLITE`, and match the outputs of the TFLite interpreter and CMSIS-NN. They rounded the exact softmax in all modes before, which differs by one step at some near-ties. In that arithmetic n is at most 4095, and the build returns `FNN_ERR_QUANT` when beta times the input scale is at most $2^{-26}$, where TFLite stops. `fnn_tflite.py` writes both tables.

## 0.4.0 (2026-11-25)

- `fnn_conv_pack_s4`, `fnn_conv_pack_s2`, `FNN_PACK_S4` and `FNN_PACK_S2` store convolution weights in [-7, 7] in 4 bits, or in {-1, 0, 1} in 2, with the same outputs as int8. A 1 x 1 layer from 64 to 64 channels takes 25% fewer cycles than with packed int8 weights, or 17% fewer when ternary.
- `fnn_dwpw_s8` takes a sub-byte 1 x 1 layer, given `FNN_DWPW_SUB_SCRATCH()` bytes of scratch. `fnn_convpool_check` and `fnn_stream_conv1d_check` return `FNN_ERR_RANGE` for sub-byte weights.
- `fnn_tflite.py` writes a convolution in 4 or 2 bits when all its weights fit, and then neither pairs it with a max pool nor streams it.

## 0.3.1 (2026-11-24)

- Packed weights are read through a `may_alias` word type. With link-time optimisation, GCC could reorder those reads and give wrong outputs from packed fully connected and convolution layers.

## 0.3.0 (2026-11-23)

- `fnn_dwpw_s8`, `fnn_dwpw_check`: a depthwise layer and the 1 x 1 convolution after it in one pass, in place. A 25 x 5 x 64 block needs 9,280 bytes instead of 16,000.
- `fnn_tflite.py` fuses a depthwise layer and its 1 x 1 convolution when that shrinks the arena. R3 needs 9,280 bytes instead of 16,064.
- `fnn_convpool_s8`, `fnn_convpool_check`: a convolution and its max pool in one pass. R5's second pair needs 2,872 bytes instead of 5,560.
- `fnn_tflite.py` also fuses a convolution and its max pool, and its report counts the pairs fused. R5 needs 2,872 bytes instead of 6,284.
- `fnn_stream.h`: `fnn_stream_conv1d_s8`, `fnn_stream_conv1d_check` and `fnn_stream_push` run causal 1-D layers one input column at a time, with the same outputs as the batch layer.
- `fnn_tflite.py --stream` writes `<name>_stream_init` and `<name>_step`, and `--check` compares them with the batch run. R2 then takes 59,044 cycles per result against 1,446,308.
- `fnn_dwconv_s8` takes 14% fewer cycles on a padded 3 x 3 layer and 6% on an unpadded one, with the same outputs.

## 0.2.0 (2026-11-23)

- `FNN_ROUND_TFLITE`, a fourth rounding mode: fully connected layers round as `FNN_ROUND_AWAY` and the other layers as `FNN_ROUND_GEMMLOWP`, the rules of the TFLite interpreter's reference kernels. A whole model then gives the interpreter's integers. The other modes compile to the same code as before.
- `nn/tools/fnn_tflite.py`: a full-integer int8 `.tflite` model to C, with the weights as constants, a memory plan for the tensors between layers and the convolutions' scratch, and `--check` against the TFLite interpreter. It converts fully connected, convolution, depthwise, pooling, mean, add, concatenation, softmax and activation layers, folds a PAD into the convolution after it, and works out the shape arithmetic TensorFlow's exporter writes. `--tflite-tables` reads activation tables from the interpreter. The memory plan writes pools, tables and adds over inputs no later layer reads, joins single rows without a copy, and is verified by replaying it before the C is written. Fully connected and convolution weights are written packed, and the generated C checks `FNN_LAYOUT_VERSION`.
- `FNN_LAYOUT_VERSION`: the order `fnn_dense_pack_s8` writes, for code that stores packed weights.
- `fnn.h`: the flexnn headers in one include.
- `fnn_maxpool_s8` and `fnn_avgpool_s8` may write over their input when no padding sits above or to the left of it.

## 0.1.0 (2026-11-22)

- `fnn_add_s8`, `fnn_add_init`: element-wise add of two int8 tensors with their own scales, optionally in place. In `FNN_ROUND_GEMMLOWP` it matches TFLite's ADD, and the other modes round the exact sum once. 30 cycles a value on the Cortex-M3 against CMSIS-NN's 108.
- `fnn_concat_s8`: concatenation along an arbitrary axis, copying by word.
- `fnn_maxpool_s8`, `fnn_avgpool_s8`, `fnn_pool_check`: max and average pooling in TFLite's layout with arbitrary window, stride and padding, matching TFLite and CMSIS-NN in all rounding modes. On the Cortex-M3 the max pool takes 15% fewer cycles than CMSIS-NN's and the average a third as many.
- `fnn_mean_s8`, `fnn_mean_check`, `fnn_mean_quant`: TFLite's MEAN over height and width, the layer Keras's global average pooling converts to, matching the TFLite interpreter in `FNN_ROUND_GEMMLOWP`.
- `fnn_dwconv_s8`, `fnn_dwconv_check`, `fnn_dwconv_fold_bias_s8`: depthwise convolution in TFLite's layout, with arbitrary filter size, stride, padding and channel multiplier, about a third fewer cycles than CMSIS-NN on the Cortex-M3.
- `fnn_conv_s8`, `fnn_conv_check`: convolution in TFLite's NHWC layout with its filter order, arbitrary filter size, stride and padding, 1 x 1 and conv1d windows read in place. Packed, it takes 12 to 20% fewer cycles than CMSIS-NN on the Cortex-M3.
- The rounding modes' documentation now names the TFLite reference kernels they match. The interpreter's fully connected layer rounds like `FNN_ROUND_AWAY`, and its convolution like `FNN_ROUND_GEMMLOWP`.
- `fnn_quantize_q15_s8`, `fnn_quantize_q31_s8`, `fnn_quantize_f32_s8`, `fnn_dequantize_s8_q15`, `fnn_dequantize_s8_f32`: a model's inputs and outputs in and out of int8, with one rounding per value. Float input takes 41 cycles a value on the Cortex-M3, against 204 for `roundf(x / s)` in soft float.
- `fnn_softmax_s8`, `fnn_argmax_s8`: softmax in TFLite's int8 format, with outputs rounded once from a 1 KB table of exponentials that `fnn_softmax_s8_build` fills, in 602 cycles for 12 scores on the Cortex-M3 against CMSIS-NN's 8159. Argmax picks the first of equal scores.
- `fnn_lut_s8`: int8 activation functions as 256-entry tables, 3.4 cycles per value on the Cortex-M3. `fnn_lut_s8_build` fills a table from a C function such as `tanh`, with entries rounded once from the exact value.
- `fnn_dense_s8`: fully connected layers, matching TFLite's reference kernel in the selected rounding mode, with the weights in TFLite's row order or reordered by `fnn_dense_pack_s8`. Packed, it takes 20 to 29% fewer cycles than CMSIS-NN's on the Cortex-M3. `fnn_fold_bias_s8` folds the input zero point into the biases.
- `fnn_requant`, `fnn_requant_s8`: int32 sums to int8 outputs with a per-tensor or per-channel multiplier, zero point and clamp. Three rounding modes (`FNN_ROUNDING`): once with ties up (the default), once with ties away from zero (the TFLite reference kernels), or gemmlowp's two roundings (CMSIS-NN's default).
- `fnn_quant_from_scale`, `fnn_quant_from_tflite`: multipliers from a real scale, or from the multiplier and shift a TFLite model stores.
