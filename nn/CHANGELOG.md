# flexnn changelog

## Unreleased

- `fnn_maxpool_s8`, `fnn_avgpool_s8`, `fnn_pool_check`: max and average pooling in TFLite's layout with any window, stride and padding, bit-exact with TFLite and CMSIS-NN in every rounding mode. On the Cortex-M3 the max pool takes 15% fewer cycles than CMSIS-NN's and the average a third as many.
- `fnn_mean_s8`, `fnn_mean_check`, `fnn_mean_quant`: TFLite's MEAN over height and width, the layer Keras's global average pooling converts to, bit-exact with the TFLite interpreter in `FNN_ROUND_GEMMLOWP`.
- `fnn_dwconv_s8`, `fnn_dwconv_check`, `fnn_dwconv_fold_bias_s8`: depthwise convolution in TFLite's layout, with any filter size, stride, padding and channel multiplier, about a third fewer cycles than CMSIS-NN on the Cortex-M3.
- `fnn_conv_s8`, `fnn_conv_check`: convolution in TFLite's NHWC layout with its filter order, any filter size, stride and padding, 1 x 1 and conv1d windows read in place. Packed, it takes 12 to 18% fewer cycles than CMSIS-NN on the Cortex-M3.
- The rounding modes' documentation now says which TFLite reference kernel each matches: the interpreter's fully connected layer rounds like `FNN_ROUND_AWAY`, its convolution like `FNN_ROUND_GEMMLOWP`.
- `fnn_quantize_q15_s8`, `fnn_quantize_q31_s8`, `fnn_quantize_f32_s8`, `fnn_dequantize_s8_q15`, `fnn_dequantize_s8_f32`: a model's inputs and outputs in and out of int8, each value rounded once. Float input takes 41 cycles a value on the Cortex-M3, against 204 for `roundf(x / s)` in soft float.
- `fnn_softmax_s8`, `fnn_argmax_s8`: softmax in TFLite's int8 format, every output rounded once from a 1 KB table of exponentials that `fnn_softmax_s8_build` fills; 602 cycles for 12 scores on the Cortex-M3 against CMSIS-NN's 8159. Argmax picks the first of equal scores, as TFLite does.
- `fnn_lut_s8`: any int8 activation function as a 256-entry table, 3.4 cycles per value on the Cortex-M3. `fnn_lut_s8_build` fills a table from a C function such as `tanh`, every entry the exact value rounded once.
- `fnn_dense_s8`: fully connected layers, bit-exact with TFLite's reference kernel in the selected rounding mode, with the weights in TFLite's row order or reordered by `fnn_dense_pack_s8`. Packed, it runs 1.25 to 1.4 times as fast as CMSIS-NN's on the Cortex-M3. `fnn_fold_bias_s8` folds the input zero point into the biases.
- `fnn_requant`, `fnn_requant_s8`: int32 sums to int8 outputs with a per-tensor or per-channel multiplier, zero point and clamp. Three rounding modes (`FNN_ROUNDING`): once with ties up (the default), once with ties away from zero (the TFLite reference kernels), or gemmlowp's two roundings (CMSIS-NN's default).
- `fnn_quant_from_scale`, `fnn_quant_from_tflite`: multipliers from a real scale, as TFLite derives them, or from TFLite's own pair.
