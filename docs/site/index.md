# flexlib

Compute libraries for Cortex-M. Every function states its error bound and
its cycle count, measured on an STM32F103.

::::{grid} 1 1 3 3
:gutter: 3

:::{grid-item-card} flexutils
:link: utils/index
:link-type: doc

Bits, text, CRCs, rings, COBS, PRNGs

```{card-footer}
CRC-32: 6.2 cycles per byte
```
:::

:::{grid-item-card} flexmath
:link: math/index
:link-type: doc

Fixed point, fast float, vectors, quaternions, PID

```{card-footer}
`fm_sqrtf`: 63 cycles, correctly rounded
```
:::

:::{grid-item-card} flexdsp
:link: dsp/index
:link-type: doc

FIR, biquads, FFT, correlation, LMS, resampling

```{card-footer}
`fd_fir_q15`: 8078 cycles, CMSIS-DSP 19863
```
:::

::::

```{toctree}
:hidden:

get-started
utils/index
math/index
dsp/index
results
design
```
