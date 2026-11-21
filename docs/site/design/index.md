# Design

These pages explain the choices that run through several modules: how sums
of products stay exact, how the FFTs scale themselves, and how every number
on this site was measured. Read them when a module page refers to them, or
when you want to know why a result comes out the way it does.

| page | contents |
|---|---|
| [Exact sums](exact-sums.md) | accumulator width, rounding and saturation of sums of products |
| [Block floating point](block-floating-point.md) | stage scaling in the FFTs and the exponent they return |
| [Measurement](measurement.md) | how the cycle counts and error bounds are measured |

```{toctree}
:hidden:

exact-sums
block-floating-point
measurement
```
