# flexdsp

Signal processing for Cortex-M on flexlib's fixed-point core: block
arithmetic, filters, transforms and generators in Q15, Q31 and float. Plain
C11, no allocation. Like the other libraries, each function states an error
bound and a cycle count measured on an STM32F103 next to CMSIS-DSP.

No functions are released yet. `fd_types.h` defines the shared types.

## Conventions

- Samples are `fm_q15`, `fm_q31` or `float`; complex samples are interleaved
  `{re, im}` pairs (`fd_cq15`, `fd_cq31`, `fd_cf32`), the CMSIS-DSP layout.
- Block functions take `(instance, x, y, n)`. `x` may equal `y` (in place);
  other overlaps are not allowed. `n == 0` does nothing.
- Instances own no memory: you pass the coefficient and state arrays, sized
  by the macros in each header.
- Init and design functions return an `FD_Status`. Programmer errors (a bad
  length, an unaligned q15 buffer) trip `FX_ASSERT`.
- On Cortex-M4 and M7 the DSP-extension instructions are used where they
  pay; results are bit-identical to the M3 build.
