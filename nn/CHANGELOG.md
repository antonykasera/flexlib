# flexnn changelog

## Unreleased

- `fnn_softmax_s8`, `fnn_argmax_s8`: softmax in TFLite's int8 format, every output rounded once from a 1 KB table of exponentials that `fnn_softmax_s8_build` fills; 602 cycles for 12 scores on the Cortex-M3 against CMSIS-NN's 8159. Argmax picks the first of equal scores, as TFLite does.
- `fnn_lut_s8`: any int8 activation function as a 256-entry table, 3.4 cycles per value on the Cortex-M3. `fnn_lut_s8_build` fills a table from a C function such as `tanh`, every entry the exact value rounded once.
- `fnn_dense_s8`: fully connected layers, bit-exact with TFLite's reference kernel in the selected rounding mode, with the weights in TFLite's row order or reordered by `fnn_dense_pack_s8`. Packed, a 256 x 64 layer runs about 1.4 times as fast as CMSIS-NN's on the Cortex-M3. `fnn_fold_bias_s8` folds the input zero point into the biases.
- `fnn_requant`, `fnn_requant_s8`: int32 sums to int8 outputs with a per-tensor or per-channel multiplier, zero point and clamp. Three rounding modes (`FNN_ROUNDING`): once with ties up (the default), once with ties away from zero (the TFLite reference kernels), or gemmlowp's two roundings (CMSIS-NN's default).
- `fnn_quant_from_scale`, `fnn_quant_from_tflite`: multipliers from a real scale, as TFLite derives them, or from TFLite's own pair.
