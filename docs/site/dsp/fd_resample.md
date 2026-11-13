<p class="fl-header-name">fd_resample.h</p>

# Resampling

Sample-rate conversion in q15 and q31: rational l / m resampling, half-band
filters by 2, and CIC decimation, with their design in float.

Rational l / m
: The prototype h runs at l times the input rate with gain l, as for
  {c:func}`fd_fir_interp_q15`, split into l branches. Only the kept outputs
  are computed, each its branch's exact sum rounded once: the same bits as
  interpolating by l and keeping every m-th output. Any block length;
  the function returns the outputs written, at most `FD_RESAMPLE_OUT_MAX`.

Half-band by 2
: A half-band lowpass of $4k - 1$ taps has the centre 1/2 and zeros at the
  other even offsets. Only the $2k$ nonzero taps are stored and multiplied,
  and the centre is an exact add (the interpolator's odd outputs are delayed
  inputs). The outputs are the same bits as the full filter through
  {c:func}`fd_fir_decim_q15`.

CIC
: `order` integrators and combs (1 to 6), adds and subtracts only, in
  registers sized from the input width: 32 bits when the result fits, which
  12-bit data does up to order 3 by 101 or order 2 by 1024, else 64. The
  q31 output keeps the bits the averaging adds and is rounded once only past
  32 bits. {c:func}`fd_cic_gain` gives the gain left, ratio^order over a
  power of two, and {c:func}`fd_cic_comp_design` an FIR that flattens the
  passband after it.

Asserts
: A factor, k or order out of range, a state buffer shorter than its
  `_STATE_LEN(..., 1)`, in_bits outside 1 to 32.

Checked
: - Resamplers bit for bit against {c:func}`fd_fir_interp_q15` then every
    m-th output, for l and m from 1 to 8 at 6 prototype lengths, narrow and
    wide coefficients, one stream cut at random into blocks of 1 to 17;
    160 / 147 and 147 / 160; nothing written past the count.
  - Half-band decimators and interpolators for k from 1 to 16 against the
    full filter through {c:func}`fd_fir_decim_q15`, and the interpolator's
    formula.
  - CIC against cascaded moving sums in 128 bits: every order, 8 ratios from
    1 to 64, 6 input widths from 1 to 32 bits; `make exhaustive`: orders 1 to
    4, ratios 2 to 16, every input width, 10^6 blocks cut at random.
  - The designs' responses: gain l at DC and -80 dB images for
    {c:func}`fd_resample_design`; 1 at DC, 0 at Nyquist and 1/2 at a
    quarter of the rate for {c:func}`fd_halfband_design`; the CIC and its
    compensator flat to 0.1 dB up to 0.15 cycles per sample.

## API

### Rational l / m

```{doxygendefine} FD_RESAMPLE_STATE_LEN
```

```{doxygendefine} FD_RESAMPLE_OUT_MAX
```

```{doxygenstruct} FD_ResampleQ15
```

```{doxygenstruct} FD_ResampleQ31
```

```{doxygenfunction} fd_resample_init_q15
```

```{doxygenfunction} fd_resample_init_q31
```

```{doxygenfunction} fd_resample_reset_q15
```

```{doxygenfunction} fd_resample_reset_q31
```

```{doxygenfunction} fd_resample_q15
```

```{doxygenfunction} fd_resample_q31
```

```{doxygenfunction} fd_resample_design
```

### Half-band by 2

```{doxygendefine} FD_HALFBAND_DECIM_STATE_LEN
```

```{doxygendefine} FD_HALFBAND_INTERP_STATE_LEN
```

```{doxygenstruct} FD_HalfbandQ15
```

```{doxygenstruct} FD_HalfbandQ31
```

```{doxygenfunction} fd_halfband_decim_init_q15
```

```{doxygenfunction} fd_halfband_decim_init_q31
```

```{doxygenfunction} fd_halfband_interp_init_q15
```

```{doxygenfunction} fd_halfband_interp_init_q31
```

```{doxygenfunction} fd_halfband_reset_q15
```

```{doxygenfunction} fd_halfband_reset_q31
```

```{doxygenfunction} fd_halfband_decim_q15
```

```{doxygenfunction} fd_halfband_decim_q31
```

```{doxygenfunction} fd_halfband_interp_q15
```

```{doxygenfunction} fd_halfband_interp_q31
```

```{doxygenfunction} fd_halfband_design
```

### CIC decimator

```{doxygendefine} FD_CIC_MAX_ORDER
```

```{doxygenstruct} FD_Cic
```

```{doxygenfunction} fd_cic_init
```

```{doxygenfunction} fd_cic_reset
```

```{doxygenfunction} fd_cic_decim_q15
```

```{doxygenfunction} fd_cic_decim_q31
```

```{doxygenfunction} fd_cic_gain
```

```{doxygenfunction} fd_cic_comp_design
```

## Numbers

64 inputs (CIC: 256), medians on an STM32F103 at 72 MHz, `-O2`, warm cache.
Errors over 10^6 half-scale inputs.

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fd_resample_q15` 3 / 2, 24 taps | {cy}`fd_resample_q15 3/2, 24 taps x 64` | {cy}`arm_fir_interpolate_q15 by 3, keep 1 in 2` | {acc}`resample q15 3/2, 24 taps; flexdsp` | {acc}`resample q15 3/2, 24 taps; CMSIS-DSP` |
| `fd_resample_q31` 3 / 2, 24 taps | {cy}`fd_resample_q31 3/2, 24 taps x 64` | {cy}`arm_fir_interpolate_q31 by 3, keep 1 in 2` | {acc}`resample q31 3/2, 24 taps; flexdsp` | {acc}`resample q31 3/2, 24 taps; CMSIS-DSP` |
| `fd_resample_q15` 160 / 147, 1280 taps | {cy}`fd_resample_q15 160/147, 1280 taps x 64` |  | 0.5 |  |
| `fd_halfband_decim_q15`, 31 taps | {cy}`fd_halfband_decim_q15, 31 taps x 64` |  | {acc}`half-band decimate q15, 31 taps; flexdsp` |  |
| `fd_halfband_decim_q31`, 31 taps | {cy}`fd_halfband_decim_q31, 31 taps x 64` |  | 0.5 |  |
| `fd_halfband_interp_q15`, 31 taps | {cy}`fd_halfband_interp_q15, 31 taps x 64` |  | {acc}`half-band interpolate q15, 31 taps; flexdsp` |  |
| `fd_halfband_interp_q31`, 31 taps | {cy}`fd_halfband_interp_q31, 31 taps x 64` |  | 0.5 |  |
| `fd_cic_decim_q15`, order 3 by 64, 12-bit | {cy}`fd_cic_decim_q15 order 3 by 64, 12-bit x 256` |  | exact |  |
| `fd_cic_decim_q15`, order 3 by 64, 16-bit | {cy}`fd_cic_decim_q15 order 3 by 64, 16-bit x 256` |  | 0.5 |  |
| `fd_cic_decim_q15`, order 5 by 16, 12-bit | {cy}`fd_cic_decim_q15 order 5 by 16, 12-bit x 256` |  | exact |  |

- CMSIS-DSP has no rational resampler: its rows interpolate by 3 with
  `arm_fir_interpolate_q15` / `_q31` and keep every second output,
  computing three outputs for every two kept. Its q31 interpolator wraps
  past full scale.
- {c:func}`fd_fir_decim_q15` and {c:func}`fd_fir_interp_q15` by 2 on the
  same 31 taps take {cy}`fd_fir_decim_q15 by 2, the same 31 taps x 64` and {cy}`fd_fir_interp_q15 by 2, the same 31 taps x 64` cycles.
- 160 / 147 is 124 cycles per output with 8 taps per branch: 8% of the
  F103 for 48 kHz out.

## Example

```{literalinclude} ../examples/fd_resample.c
:language: c
```

## How it works

Outputs l apart at the high rate use the same branch. When a block holds at
least four outputs per branch, they go through the decimator kernel four at
a time; otherwise one by one, which is the case for 160 / 147. Each CIC
decimator is about 4 KB of code: a loop per order and register width.
