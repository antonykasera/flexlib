# flexnn changelog

## 0.3.0 (2026-11-23)

- `fnn_dwpw_s8`, `fnn_dwpw_check`: a depthwise layer and the 1 x 1 convolution after it in one pass, in place. A 25 x 5 x 64 block needs 9,280 bytes instead of 16,000.
- `fnn_tflite.py` fuses a depthwise layer and its 1 x 1 convolution when that shrinks the arena. R3 needs 9,280 bytes instead of 16,064.
- `fnn_convpool_s8`, `fnn_convpool_check`: a convolution and its max pool in one pass. R5's second pair needs 2,872 bytes instead of 5,560.
- `fnn_tflite.py` also fuses a convolution and its max pool, and its report counts the pairs fused. R5 needs 2,872 bytes instead of 6,284.
- `fnn_stream.h`: `fnn_stream_conv1d_s8`, `fnn_stream_conv1d_check` and `fnn_stream_push` run causal 1-D layers one input column at a time, bit-exact with the batch layer.
- `fnn_tflite.py --stream` writes `<name>_stream_init` and `<name>_step`, and `--check` compares them with the batch run. R2 then takes 59,044 cycles per result against 1,446,308.
- `fnn_dwconv_s8` takes 14% fewer cycles on a padded 3 x 3 layer and 6% on an unpadded one, with the same outputs.

## 0.2.0 (2026-11-23)

- `FNN_ROUND_TFLITE`, a fourth rounding mode: fully connected layers round as `FNN_ROUND_AWAY` and every other layer as `FNN_ROUND_GEMMLOWP`, the rules of the TFLite interpreter's reference kernels. A whole model then gives the interpreter's integers. The other modes compile to the same code as before.
- `nn/tools/fnn_tflite.py`: a full-integer int8 `.tflite` model to C, with the weights as constants, a memory plan for the tensors between layers and the convolutions' scratch, and `--check` against the TFLite interpreter. It converts fully connected, convolution, depthwise, pooling, mean, add, concatenation, softmax and activation layers, folds a PAD into the convolution after it, and works out the shape arithmetic TensorFlow's exporter writes. `--tflite-tables` reads activation tables from the interpreter. The memory plan writes pools, tables and adds over inputs no later layer reads, joins single rows without a copy, and is replayed byte by byte before any C is written. Fully connected and convolution weights are written packed, and the generated C checks `FNN_LAYOUT_VERSION`.
- `FNN_LAYOUT_VERSION`: the order `fnn_dense_pack_s8` writes, for code that stores packed weights.
- `fnn.h`: every flexnn header in one include.
- `fnn_maxpool_s8` and `fnn_avgpool_s8` may write over their input when no padding sits above or to the left of it.

## 0.1.0 (2026-11-22)

- `fnn_add_s8`, `fnn_add_init`: element-wise add of two int8 tensors with their own scales, in place if you like. In `FNN_ROUND_GEMMLOWP` it is TFLite's ADD bit for bit; the other modes round the exact sum once. 30 cycles a value on the Cortex-M3 against CMSIS-NN's 108.
- `fnn_concat_s8`: concatenation along any axis, copying by word.
- `fnn_maxpool_s8`, `fnn_avgpool_s8`, `fnn_pool_check`: max and average pooling in TFLite's layout with any window, stride and padding, bit-exact with TFLite and CMSIS-NN in every rounding mode. On the Cortex-M3 the max pool takes 15% fewer cycles than CMSIS-NN's and the average a third as many.
- `fnn_mean_s8`, `fnn_mean_check`, `fnn_mean_quant`: TFLite's MEAN over height and width, the layer Keras's global average pooling converts to, bit-exact with the TFLite interpreter in `FNN_ROUND_GEMMLOWP`.
- `fnn_dwconv_s8`, `fnn_dwconv_check`, `fnn_dwconv_fold_bias_s8`: depthwise convolution in TFLite's layout, with any filter size, stride, padding and channel multiplier, about a third fewer cycles than CMSIS-NN on the Cortex-M3.
- `fnn_conv_s8`, `fnn_conv_check`: convolution in TFLite's NHWC layout with its filter order, any filter size, stride and padding, 1 x 1 and conv1d windows read in place. Packed, it takes 12 to 20% fewer cycles than CMSIS-NN on the Cortex-M3.
- The rounding modes' documentation now says which TFLite reference kernel each matches: the interpreter's fully connected layer rounds like `FNN_ROUND_AWAY`, its convolution like `FNN_ROUND_GEMMLOWP`.
- `fnn_quantize_q15_s8`, `fnn_quantize_q31_s8`, `fnn_quantize_f32_s8`, `fnn_dequantize_s8_q15`, `fnn_dequantize_s8_f32`: a model's inputs and outputs in and out of int8, each value rounded once. Float input takes 41 cycles a value on the Cortex-M3, against 204 for `roundf(x / s)` in soft float.
- `fnn_softmax_s8`, `fnn_argmax_s8`: softmax in TFLite's int8 format, every output rounded once from a 1 KB table of exponentials that `fnn_softmax_s8_build` fills; 602 cycles for 12 scores on the Cortex-M3 against CMSIS-NN's 8159. Argmax picks the first of equal scores.
- `fnn_lut_s8`: any int8 activation function as a 256-entry table, 3.4 cycles per value on the Cortex-M3. `fnn_lut_s8_build` fills a table from a C function such as `tanh`, every entry the exact value rounded once.
- `fnn_dense_s8`: fully connected layers, bit-exact with TFLite's reference kernel in the selected rounding mode, with the weights in TFLite's row order or reordered by `fnn_dense_pack_s8`. Packed, it runs 1.25 to 1.4 times as fast as CMSIS-NN's on the Cortex-M3. `fnn_fold_bias_s8` folds the input zero point into the biases.
- `fnn_requant`, `fnn_requant_s8`: int32 sums to int8 outputs with a per-tensor or per-channel multiplier, zero point and clamp. Three rounding modes (`FNN_ROUNDING`): once with ties up (the default), once with ties away from zero (the TFLite reference kernels), or gemmlowp's two roundings (CMSIS-NN's default).
- `fnn_quant_from_scale`, `fnn_quant_from_tflite`: multipliers from a real scale, or from the multiplier and shift a TFLite model stores.
