<p class="fl-header-name">fd_filt.h</p>

# Small filters

Not every signal needs a designed filter. Often you want to remove a DC
offset, smooth a sensor reading, average the last few samples, or drop the
occasional spike. `fd_filt.h` has those: a DC blocker, a one-pole smoother,
a moving average, and medians of 3 and 5.

## Cleaning up a sensor reading

The reading here wanders between 1000 and 1003, with one spike to full
scale:

```{literalinclude} ../examples/fd_filt.c
:language: c
```

Which prints

```{literalinclude} ../examples/fd_filt.out
:language: text
```

The median of 5 replaces the spike at sample 17 with 1003, an ordinary
reading, where an average would have smeared it over its neighbours. The
moving average of 4 then smooths what is left, and its largest output is
1002.

## The filters

The DC blocker computes $y = x - x1 + a y1$. Because $x - x1$ is an
integer, adding the rounded $a y1$ rounds the exact result once.

The one-pole smoother computes $y += a (x - y)$ with $a >= 0$: y moves
towards x and never passes it. {c:func}`fd_onepole_coef_q15` gives the a for
a -3 dB corner frequency, which must lie in $(0, 0.5)$.

The moving average is the mean of the last n inputs, with zeros before the
first one. It keeps an exact running sum, so it does not drift, and a
power-of-two n divides by a shift. In q15 the window can be up to 65536
samples.

The medians {c:func}`fd_median3` and `fd_median5` are branchless, so they
take the same time whatever the input.

Every output is rounded once, within 0.5 LSB, and saturated, and `y` may be
the same buffer as `x`. A negative a, a corner outside $(0, 0.5)$ and a q15
window past 65536 stop at an assert.

## How it was checked

- The DC blocker, one-pole and moving average bit for bit against 128-bit
  references, on random and alternating full-scale input, with 40 random
  coefficients and the largest, windows of 1 to 300 and blocks cut at
  random. The 65536-sample q15 window at full scale.
- {c:func}`fd_onepole_coef_q31` puts the response at -3.01 dB at 0.001, 0.05
  and 0.3 cycles per sample, and the q15 form within 1 LSB of it.
- The medians against sorting, on random and extreme values.

## Numbers

Blocks of 64, medians on an STM32F103 at 72 MHz, `-O2`, warm cache.

| function | cycles |
|---|--:|
| `fd_dcblock_q15` | {cy}`fd_dcblock_q15, 64` |
| `fd_dcblock_q31` | {cy}`fd_dcblock_q31, 64` |
| `fd_onepole_q15` | {cy}`fd_onepole_q15, 64` |
| `fd_onepole_q31` | {cy}`fd_onepole_q31, 64` |
| `fd_movavg_q15`, $n = 32$ | {cy}`fd_movavg_q15 n = 32, 64` |
| `fd_movavg_q15`, $n = 30$ | {cy}`fd_movavg_q15 n = 30, 64` |
| `fd_movavg_q31`, $n = 32$ | {cy}`fd_movavg_q31 n = 32, 64` |
| `fd_median5`, one call | {cy}`fd_median5` |

## API

### DC blocker

```{doxygenstruct} FD_DcBlockQ15
```

```{doxygenstruct} FD_DcBlockQ31
```

```{doxygenfunction} fd_dcblock_init_q15
```

```{doxygenfunction} fd_dcblock_init_q31
```

```{doxygenfunction} fd_dcblock_q15
```

```{doxygenfunction} fd_dcblock_q31
```

### One-pole smoother

```{doxygenstruct} FD_OnePoleQ15
```

```{doxygenstruct} FD_OnePoleQ31
```

```{doxygenfunction} fd_onepole_init_q15
```

```{doxygenfunction} fd_onepole_init_q31
```

```{doxygenfunction} fd_onepole_q15
```

```{doxygenfunction} fd_onepole_q31
```

```{doxygenfunction} fd_onepole_coef_q15
```

```{doxygenfunction} fd_onepole_coef_q31
```

### Moving average

```{doxygenstruct} FD_MovAvgQ15
```

```{doxygenstruct} FD_MovAvgQ31
```

```{doxygenfunction} fd_movavg_init_q15
```

```{doxygenfunction} fd_movavg_init_q31
```

```{doxygenfunction} fd_movavg_q15
```

```{doxygenfunction} fd_movavg_q31
```

### Medians

```{doxygenfunction} fd_median3
```

```{doxygenfunction} fd_median5
```
