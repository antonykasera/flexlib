<p class="fl-header-name">fd_tone.h</p>

# Tones

Some jobs need one frequency rather than a whole spectrum: detecting a DTMF
key, checking a pilot tone, generating a test signal. `fd_tone.h` has the
Goertzel detector, which measures one frequency much more cheaply than an
FFT, a numerically controlled oscillator (NCO) that generates sine and
cosine, and a recursive oscillator for when even the NCO is too costly.

## Detecting a DTMF key

DTMF key 5 is 770 Hz plus 1336 Hz. This generates it with two NCOs at
8 kHz, then asks two Goertzel detectors whether 770 Hz and 852 Hz are
present:

```{literalinclude} ../examples/fd_tone.c
:language: c
```

Which prints

```{literalinclude} ../examples/fd_tone.out
:language: text
```

The 770 Hz detector sees 25.2 dB more power than the 852 Hz one, so the key
is in the 770 Hz row. 205 samples is the block length DTMF decoders usually
use at 8 kHz.

## Frequencies

Frequencies are phase steps per sample, with a full turn being 2^32: f Hz
at a sample rate fs is $f / fs 2^32$, as the example's `STEP` macro
computes. A Goertzel detector takes any frequency, not only the center of a
DFT bin.

## Goertzel

The detector keeps a 32-bit state. The inputs enter it as $x 2^shift$ in
q15 or $x / 2^shift$ in q31, and the state wraps past 2^31, so choose
`shift` to keep $n |x| / |sin(w)| < 2^31$ in state units. Low frequencies,
where $sin(w)$ is small, need the most room. {c:func}`fd_goertzel_dft`
gives the DFT term at the end of the block, and {c:func}`fd_goertzel_power`
its squared magnitude, computed exactly from the state. A shift past 31
(q15 input: 16) stops at an assert.

## Oscillators

The NCO feeds a 32-bit phase accumulator into {c:func}`fm_sincos_q31`. q31
outputs are within 1.11 LSB, and q15 outputs are those values rounded. The
phase never drifts, and setting `dinc` adds a step to the frequency every
sample, which makes a linear chirp.

The recursive oscillator rotates a vector by a fixed angle each sample in
Q30, and renormalises it every 256 samples. Over 10^6 samples its amplitude
stays within 10^-6 of 1, and its error under 10^-4 of full scale.

## How it was checked

- The Goertzel state bit for bit against a 32-bit model at 40 random
  frequencies and shifts, fed in pieces. The power against the exact value
  from the state, and the DFT term within 10^-4 of a double DFT.
- The NCO outputs bit for bit against {c:func}`fm_sin_q31` and
  {c:func}`fm_cos_q31` over 300 samples of a chirp, in every output form.
- The oscillator's amplitude and error over 10^6 samples, against `sin`.

## Numbers

Blocks of 64, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles |
|---|--:|
| `fd_goertzel_q15` | {cy}`fd_goertzel_q15, 64` |
| `fd_goertzel_q31` | {cy}`fd_goertzel_q31, 64` |
| `fd_nco_q15`, sin | {cy}`fd_nco_q15 sin, 64` |
| `fd_nco_q31`, sin and cos | {cy}`fd_nco_q31 sin + cos, 64` |
| `fd_osc_q15`, sin | {cy}`fd_osc_q15 sin, 64` |
| `fd_osc_q31`, sin and cos | {cy}`fd_osc_q31 sin + cos, 64` |

A 16-bit phase into {c:func}`fm_sin_q15` takes {cy}`nco q15, 16-bit phase into fm_sin_q15` cycles for the same 64
samples, with errors to 1.6 LSB.

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
