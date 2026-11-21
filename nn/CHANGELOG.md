# flexnn changelog

## Unreleased

- `fnn_requant`, `fnn_requant_s8`: int32 sums to int8 outputs with a per-tensor or per-channel multiplier, zero point and clamp. Three rounding modes (`FNN_ROUNDING`): once with ties up (the default), once with ties away from zero (the TFLite reference kernels), or gemmlowp's two roundings (CMSIS-NN's default).
- `fnn_quant_from_scale`, `fnn_quant_from_tflite`: multipliers from a real scale, as TFLite derives them, or from TFLite's own pair.
