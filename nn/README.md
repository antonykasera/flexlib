# flexnn

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](../LICENSE)

flexnn runs small quantised neural networks on Cortex-M: int8 inference with
the arithmetic of TFLite's int8 models. A network you train and quantise on a
PC gives the same integers on the board. It is in development and not part
of a release yet. What is here so far is the step every layer ends with:
turning a layer's int32 sums into int8 outputs.

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

Every int32 sum, for 15 multiplier and shift pairs at the ends of their
ranges, in each rounding mode, against an exact 64-bit model. Random sums,
multipliers and every shift against a second model written in TFLite's and
gemmlowp's own terms. `fnn_quant_from_scale` against TFLite's
`QuantizeMultiplier` on 100,000 random scales and the edge cases (scales that
round up to the next power of two, scales too small or too large to
represent). [docs/testing.md](../docs/testing.md) lists the commands.
