<p class="fl-header-name">fd_biquad.h</p>

# Biquads

A biquad is a second-order IIR filter section, and a cascade of them builds
any IIR filter: a Butterworth lowpass, a notch, the shelves and peaks of an
audio EQ. They need far fewer multiplies than a FIR with the same
sharpness. `fd_biquad.h` runs cascades in q15, q31 and float, and designs
the sections for you.

## A Butterworth lowpass

This designs a 4th-order Butterworth lowpass at 0.01 cycles per sample, two
sections, and steps its input to 0.5:

```{literalinclude} ../examples/fd_biquad.c
:language: c
```

Which prints

```{literalinclude} ../examples/fd_biquad.out
:language: text
```

{c:func}`fd_biquad_butter` designs the sections in float, and
{c:func}`fd_biquad_quantize_q31` rounds them to Q2.30 and picks `bshift`,
the scaling of the b coefficients, here 0. After 1000 samples the output has
settled 541 LSB below 0.5 in q31: the rounded coefficients move the DC gain
by about 5 parts in 10^7.

## Coefficients

Each section takes b0, b1, b2, a1 and a2, with
$y = b0 x + b1 x1 + b2 x2 + a1 y1 + a2 y2$: the order and signs CMSIS-DSP
uses. They are Q2.30 in q31 and Q2.14 in q15. A section with a large gain
stores its b coefficients divided by 2^bshift, so the gain costs only the b
part, and the feedback keeps full precision.

The design functions hold the poles as $d1 = 2 - a1$ and $d2 = a2 + 1$, which
stay exact near $z = 1$, where low-frequency filters put them. The quantize
functions round the design, pick the smallest bshift, and report sections
that rounding has made unstable.

## Rounding and error feedback

Each section's output is its exact sum rounded once and saturated, for any
coefficients. In a recursive filter that rounding is fed back, and at a very
low corner it shows up as noise near DC. {c:func}`fd_biquad_init_ef_q31`
adds first-order error feedback, which moves that noise away from DC.
Use it in q31 for corners well below 0.01 cycles per sample.

## Float

{c:func}`fd_biquad_f32` takes the same coefficients as floats. Each
section's output is the exact sum of its products, rounded once, within
0.5 ulp + $2^-32 sum |p|$. It is about twice as fast as
`arm_biquad_cascade_df2T_f32` in soft float, and more accurate. A NaN or inf,
in the input or from a section that overflows, makes every later output NaN
until you reset the filter. `nsec` of 0 and a bshift past 31 (q15: 15) stop
at an assert.

## How it was checked

- Bit for bit against the cascade in 128 bits: 1 to 4 sections of designed,
  random narrow and random wide coefficients, plain and with error feedback,
  blocks cut at random, in place and after a reset.
- The boundaries: a sum of |c| of exactly 4, sections whose b and a parts are
  each below 4 but whose total passes it, and saturation at full scale.
- The designs against RBJ in double at 5 frequencies from 10^-4 to 0.45, and
  an 8th-order Butterworth at -3.01 dB at its corner.
- Error feedback 40 dB quieter than the plain cascade on a 4th-order lowpass
  at 5 10^-4.
- {c:func}`fd_biquad_f32` bit for bit against each section's sum in
  `__float128` rounded once: 8th-order Butterworth lowpasses at 0.01, 0.1
  and 0.3, 4000 samples each with the level stepped from 2^-20 to 2^29, and
  the edge cases.

## Numbers

Four sections (an 8th-order Butterworth lowpass), blocks of 64, medians on
an STM32F103 at 72 MHz, `-O2`, warm cache. Errors are the recursion's,
against the same coefficients in double, over 10^6 samples of half-scale
noise through a 4th-order Butterworth at 0.01 cycles per sample.

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_biquad_q15` | {cy}`fd_biquad_q15, 4 sections x 64` | {cy}`arm_biquad_cascade_df1_q15, 4 x 64` | {acc}`biquad q15, Butterworth 4, fc 0.01; flexdsp` | {acc}`biquad q15, Butterworth 4, fc 0.01; CMSIS-DSP` |
| `fd_biquad_q15`, $sum \|c\| >= 4$ | {cy}`fd_biquad_q15, sum of c >= 4 (64-bit)` | | | |
| `fd_biquad_q31` | {cy}`fd_biquad_q31, 4 sections x 64` | {cy}`arm_biquad_cascade_df1_q31, 4 x 64` | {acc}`biquad q31, Butterworth 4, fc 0.01; flexdsp` | {acc}`biquad q31, Butterworth 4, fc 0.01; CMSIS-DSP` |
| `fd_biquad_q31`, error feedback | {cy}`fd_biquad_q31 error feedback, 4 x 64` | {cy}`arm_biquad_cas_df1_32x64_q31, 4 x 64` | {acc}`biquad q31, Butterworth 4, fc 0.01; flexdsp error feedback` | {acc}`biquad q31, Butterworth 4, fc 0.01; CMSIS-DSP 32x64` |

CMSIS-DSP: `arm_biquad_cascade_df1_q15` and `_q31`; error feedback against
`arm_biquad_cas_df1_32x64_q31`, which keeps 64-bit state. The `fast` forms
take {cy}`arm_biquad_cascade_df1_fast_q15, 4 x 64` (q15) and {cy}`arm_biquad_cascade_df1_fast_q31, 4 x 64` (q31) cycles. At a corner of 5 10^-4 the plain q31
cascade reaches {acc}`biquad q31, Butterworth 4, fc 5e-4; flexdsp` LSB and error feedback
{acc}`biquad q31, Butterworth 4, fc 5e-4; flexdsp error feedback`; CMSIS-DSP q31 {acc}`biquad q31, Butterworth 4, fc 5e-4; CMSIS-DSP`, its 32x64 form
{acc}`biquad q31, Butterworth 4, fc 5e-4; CMSIS-DSP 32x64`.

A 48 kHz EQ of four sections (low shelf, two peaks, high shelf), blocks of 64:

| bshift | q15 | q31 | q31, error feedback |
|---|--:|--:|--:|
| 0 | {cy}`fd_biquad_q15, 48 kHz EQ, 4 x 64` | {cy}`fd_biquad_q31, 48 kHz EQ, 4 x 64` | {cy}`fd_biquad_q31 error feedback, 48 kHz EQ, 4 x 64` |
| 1 (a +12 dB high shelf) | {cy}`fd_biquad_q15, 48 kHz EQ, bshift 1, 4 x 64` | {cy}`fd_biquad_q31, 48 kHz EQ, bshift 1, 4 x 64` |  |

Float, an 8th-order Butterworth lowpass, soft float on the F103. Errors are
in ulp of the exact cascade, over 10^6 samples of white noise.

| function | cycles | CMSIS-DSP | mean error, ulp | CMSIS-DSP, ulp |
|---|--:|--:|--:|--:|
| `fd_biquad_f32`, fc 0.1 | {cy}`fd_biquad_f32, 4 x 64` | {cy}`arm_biquad_cascade_df2T_f32, 4 x 64` | {acc}`biquad f32, Butterworth 8, fc 0.1; flexdsp @ mean` | {acc}`biquad f32, Butterworth 8, fc 0.1; CMSIS-DSP @ mean` |
| `fd_biquad_f32`, fc 0.01 |  |  | {acc}`biquad f32, Butterworth 8, fc 0.01; flexdsp @ mean` | {acc}`biquad f32, Butterworth 8, fc 0.01; CMSIS-DSP @ mean` |

CMSIS-DSP: `arm_biquad_cascade_df2T_f32`, its fastest float form. The
recursion amplifies each section's rounding, more at the lower corner.

## How it works

When a section's $|b0| + |b1| + |b2| + |a1| + |a2| < 4$, its sum fits
64 bits (q15: 32) and runs in one accumulator. Shelves and peaks usually
pass 4; their b and a parts are then summed apart, each exact in 64 bits
(q15: 32), and added with an overflow check. Only sections whose stored b or
a alone reach 4 take 96-bit (q15: 64-bit) sums. Init picks the kernel per
section.

The float cascade unpacks each coefficient once into a mantissa with 6 bits
of headroom and an exponent, and holds the history unpacked the same way.
A section's five products are exact, aligned to the largest and summed in
64 bits, then rounded once to float.

## API

```{doxygendefine} FD_BIQUAD_STATE_LEN
```

```{doxygenstruct} FD_BiquadQ31
```

```{doxygenstruct} FD_BiquadQ15
```

```{doxygenfunction} fd_biquad_init_q31
```

```{doxygenfunction} fd_biquad_init_q15
```

```{doxygenfunction} fd_biquad_init_ef_q31
```

```{doxygenfunction} fd_biquad_reset_q31
```

```{doxygenfunction} fd_biquad_reset_q15
```

```{doxygenfunction} fd_biquad_q31
```

```{doxygenfunction} fd_biquad_q15
```

### Float

```{doxygendefine} FD_BIQUAD_F32_STATE_LEN
```

```{doxygenstruct} FD_BiquadF32
```

```{doxygenfunction} fd_biquad_init_f32
```

```{doxygenfunction} fd_biquad_reset_f32
```

```{doxygenfunction} fd_biquad_f32
```

### Design

```{doxygenenum} FD_BiquadType
```

```{doxygenstruct} FD_BiquadDesign
:members:
```

```{doxygenfunction} fd_biquad_design
```

```{doxygenfunction} fd_biquad_butter
```

```{doxygenfunction} fd_biquad_quantize_q31
```

```{doxygenfunction} fd_biquad_quantize_q15
```
