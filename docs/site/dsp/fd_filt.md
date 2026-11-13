<p class="fl-header-name">fd_filt.h</p>

# Small filters

A DC blocker, a one-pole smoother, a moving average and branchless medians
of 3 and 5.

Rounding
: Outputs are rounded once (0.5 LSB) and saturated; `y` may equal `x`.

DC blocker
: $y = x - x1 + a y1$. $x - x1$ is an integer, so adding the rounded $a y1$
  rounds the exact value once.

One-pole
: $y += a (x - y)$ with $a >= 0$: y moves towards x and never passes it.
  {c:func}`fd_onepole_coef_q15` gives a for a -3 dB corner.

Moving average
: The mean of the last n inputs, zeros before the first, from an exact
  running sum; a power-of-two n divides by shifting.

Asserts
: $a < 0$, a corner outside $(0, 0.5)$, a q15 window past 65536.

Checked
: - DC blocker, one-pole and moving average bit for bit against 128-bit
    references, on random and alternating full-scale input, with 40 random
    coefficients and the largest, windows of 1 to 300, blocks cut at
    random; the 65536-sample q15 window at full scale.
  - {c:func}`fd_onepole_coef_q31` puts the response at -3.01 dB at 0.001,
    0.05 and 0.3 cycles per sample; the q15 form within 1 LSB of it.
  - The medians against sorting, on random and extreme values.

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

## Example

```{literalinclude} ../examples/fd_filt.c
:language: c
```
