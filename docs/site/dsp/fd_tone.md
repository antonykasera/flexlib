<p class="fl-header-name">fd_tone.h</p>

# Tones

Goertzel detection of one frequency, a numerically controlled oscillator
(NCO), and a recursive oscillator.

Frequencies
: Phase steps per sample, a full turn being 2^32: f Hz at a rate fs is
  f / fs 2^32. A Goertzel detector takes any frequency, not only a DFT
  bin.

Goertzel
: Inputs enter the 32-bit state as x 2^shift (q15) or x / 2^shift (q31),
  and the state wraps past 2^31: keep n |x| / |sin w| under 2^31 in state
  units. {c:func}`fd_goertzel_dft` gives the DFT term, and
  {c:func}`fd_goertzel_power` its squared magnitude, exact from the state.

NCO
: A 32-bit phase accumulator into {c:func}`fm_sincos_q31`: q31 within
  1.11 LSB, q15 those values rounded. The phase never drifts, and `dinc`
  makes a linear chirp.

Oscillator
: A rotation per sample in Q30, renormalised every 256 samples: over 10^6
  samples the amplitude stays within 10^-6 of 1 and the error under 10^-4
  of full scale.

Asserts
: A Goertzel shift past 31 (q15 input: 16).

Checked
: - Goertzel state bit for bit against a 32-bit model at 40 random
    frequencies and shifts, fed in pieces; the power against the exact
    value from the state; the DFT term within 10^-4 of a double DFT.
  - NCO outputs bit for bit against {c:func}`fm_sin_q31` and
    {c:func}`fm_cos_q31` over 300 samples of a chirp, every output form.
  - Oscillator amplitude and error over 10^6 samples, against `sin`.

## API

### Goertzel

```{doxygenstruct} FD_Goertzel
```

```{doxygenfunction} fd_goertzel_init
```

```{doxygenfunction} fd_goertzel_reset
```

```{doxygenfunction} fd_goertzel_q15
```

```{doxygenfunction} fd_goertzel_q31
```

```{doxygenfunction} fd_goertzel_dft
```

```{doxygenfunction} fd_goertzel_power
```

### NCO

```{doxygenstruct} FD_Nco
:members:
```

```{doxygenfunction} fd_nco_init
```

```{doxygenfunction} fd_nco_q15
```

```{doxygenfunction} fd_nco_q31
```

```{doxygenfunction} fd_nco_cplx_q15
```

```{doxygenfunction} fd_nco_cplx_q31
```

### Recursive oscillator

```{doxygenstruct} FD_Osc
```

```{doxygenfunction} fd_osc_init
```

```{doxygenfunction} fd_osc_q15
```

```{doxygenfunction} fd_osc_q31
```

## Numbers

Blocks of 64, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles |
|---|--:|
| `fd_goertzel_q15` | 1374 |
| `fd_goertzel_q31` | 1921 |
| `fd_nco_q15`, sin | 5303 |
| `fd_nco_q31`, sin and cos | 7067 |
| `fd_osc_q15`, sin | 2694 |
| `fd_osc_q31`, sin and cos | 3925 |

A 16-bit phase into {c:func}`fm_sin_q15` takes 2400 cycles for the same 64
samples, with errors to 1.6 LSB.

## Example

```{literalinclude} ../examples/fd_tone.c
:language: c
```
