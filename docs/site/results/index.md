# Results

These pages list every case the bench firmware times, next to the baselines
it was measured against: newlib, libgcc's soft float and CMSIS-DSP, built
with the same flags in the same firmware. The module pages pick out the main
cases. Come here when you want the case that matches your own use, or the
spread between the fastest and slowest call.

Board
: STM32F103 (Cortex-M3) at 72 MHz: CPUID {bm}`CPUID`.

Build
: {bm}`build`.

Method
: Cycles per call {bm}`cycles per call`. Each table gives the minimum, median and maximum
  over the samples. [Measurement](../design/measurement.md) describes the
  method and what the numbers include.

Results
: Every flexlib row passed its golden check: the output hash matches the
  host build's.

| page | contents |
|---|---|
| [flexutils](utils.md) | bits, formatting, parsing, CRCs, rings, COBS, debouncing, random numbers |
| [flexmath](math.md) | fixed point, square roots, division, trigonometry, logarithms, float functions, linear algebra, PID |
| [flexdsp](dsp.md) | vectors, filters, windows, tones, noise, FFTs, correlation, LMS, resampling |

## Overhead

The cost of the timing itself, already subtracted from every other row, and
an empty call.

:::{bench-rows}
CYCCNT read
empty
:::

## Shared layer

The intrinsics of `fx_arch.h`, the layer under all three libraries, one
call each.

:::{bench-rows} fx_arch.h
FX_SSAT
FX_USAT
:::

## Soft-float operations

Single operations through libgcc, for scale.

:::{bench-rows}
float multiply
float divide
:::

```{toctree}
:hidden:

utils
math
dsp
```
