<p class="fl-header-name">fm_trig.h</p>

# Sine and cosine

Tone generators, mixers and motor control all need sine and cosine of an
angle that keeps advancing. `fm_trig.h` computes them for binary angles in
Q15 and Q31. In a binary angle a full turn is 2^16 (Q15) or 2^32 (Q31), so an
unsigned phase variable wraps round at exactly one turn by itself, and you
never reduce the angle.

## A 1 kHz oscillator

At 48 kHz a 1 kHz tone advances by 1000 / 48000 of a turn per sample, so the
Q31 phase steps by $2^32 1000 / 48000$:

```{literalinclude} ../examples/fm_trig.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_trig.out
:language: text
```

Over ten periods the sine and cosine stay within 1 LSB of `double`'s, the
`1.000` in the output, inside the 1.11 LSB bound.
{c:func}`fm_sincos_q31` gives both from one call. The last line takes the
Q15 sine of a quarter turn, 0x4000. The answer is 1, which Q15 cannot hold,
so it saturates to 32767.

## Accuracy

The Q15 functions are within 0.66 LSB, and the Q31 ones within 1.11 LSB.
{c:func}`fm_sincos_q31` gives the same bits as {c:func}`fm_sin_q31` and
{c:func}`fm_cos_q31` would, in less time than the two calls.

The functions use small tables. If a call finds them outside the flash
cache, it costs more, as the numbers below show. `FM_LUT_IN_RAM` moves the
tables to RAM for that case.

## How it was checked

- Q15 sine and cosine on all 65536 angles, and Q31 sine on all 2^32,
  against `long double`.
- The Q31 cosine bit for bit against the sine a quarter turn on, and the
  pair functions bit for bit against the single calls.

## Numbers

| function | cycles | CMSIS-DSP | max error, LSB | CMSIS-DSP, LSB |
|---|--:|--:|--:|--:|
| `fm_sin_q15` | {cy}`fm_sin_q15` | {cy}`arm_sin_q15` | {acc}`sin q15; flexmath` | {acc}`sin q15; CMSIS-DSP` |
| `fm_cos_q15` | {cy}`fm_cos_q15` |  | 0.65 |  |
| `fm_sincos_q15` | {cy}`fm_sincos_q15` |  | 0.65 |  |
| `fm_sin_q31` | {cy}`fm_sin_q31` | {cy}`arm_sin_q31` | 1.105 | {acc}`sin q31; CMSIS-DSP` |
| `fm_cos_q31` | {cy}`fm_cos_q31` |  | 1.105 |  |
| `fm_sincos_q31` | {cy}`fm_sincos_q31` |  | 1.105 |  |

Medians over 256 inputs on an STM32F103 at 72 MHz, `-O2`, warm cache.
CMSIS-DSP: `arm_sin_q15` and `arm_sin_q31`. With the flash cache flushed
before each call, {c:func}`fm_sin_q15` takes {cy}`fm_sin_q15 @ cold` cycles and
{c:func}`fm_sin_q31` {cy}`fm_sin_q31 @ cold`; `FM_LUT_IN_RAM` moves the tables to RAM.

## API

```{doxygenfunction} fm_sin_q15
```

```{doxygenfunction} fm_cos_q15
```

```{doxygenfunction} fm_sincos_q15
```

```{doxygenfunction} fm_sin_q31
```

```{doxygenfunction} fm_cos_q31
```

```{doxygenfunction} fm_sincos_q31
```
