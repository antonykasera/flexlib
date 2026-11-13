<p class="fl-header-name">fd_biquad.h</p>

# Biquads

Cascades of second-order sections in direct form 1, q15 and q31, and their
design in float: RBJ cookbook sections and Butterworth cascades.

Coefficients
: Per section b0 b1 b2 a1 a2, with $y = b0 x + b1 x1 + b2 x2 + a1 y1 + a2 y2$:
  the CMSIS-DSP order and signs. Q2.30 (q31) or Q2.14 (q15). The b are
  stored divided by 2^bshift, so a large gain costs the b alone and the
  feedback keeps full precision.

Rounding
: Each section's output is its exact sum rounded once (0.5 LSB) and
  saturated, for any coefficients.

Error feedback
: {c:func}`fd_biquad_init_ef_q31` adds first-order error feedback, which
  moves the rounding noise away from DC: for low corners in q31.

Design
: `FD_BiquadDesign` holds the poles as $d1 = 2 - a1$ and
  $d2 = a2 + 1$, which stay exact near $z = 1$. The quantize functions round,
  pick the smallest bshift, and report sections that rounding made unstable.

Asserts
: `nsec` 0, bshift past 31 (q15: 15).

Checked
: - Bit for bit against the cascade in 128 bits: 1 to 4 sections of
    designed, random narrow and random wide coefficients, plain and with
    error feedback, blocks cut at random, in place, after a reset.
  - The boundaries: $sum |c| = 4$, sections with $sum |b| < 4$ and $sum |a| < 4$
    but $sum |c| >= 4$, saturation at full scale.
  - The designs against RBJ in double at 5 frequencies from 10^-4 to 0.45;
    an 8th-order Butterworth at -3.01 dB at its corner.
  - Error feedback 40 dB quieter than the plain cascade on a 4th-order
    lowpass at 5 10^-4.

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

## Example

```{literalinclude} ../examples/fd_biquad.c
:language: c
```

## How it works

When a section's $|b0| + |b1| + |b2| + |a1| + |a2| < 4$, its sum fits
64 bits (q15: 32) and runs in one accumulator. Shelves and peaks usually
pass 4; their b and a parts are then summed apart, each exact in 64 bits
(q15: 32), and added with an overflow check. Only sections whose stored b or
a alone reach 4 take 96-bit (q15: 64-bit) sums. Init picks the kernel per
section.
