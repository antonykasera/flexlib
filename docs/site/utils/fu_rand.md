<p class="fl-header-name">fu_rand.h</p>

# Random numbers

Pseudo-random numbers for simulation, test data and jitter. Use xorshift32
when speed is required. Use xoshiro128** for better statistics and separate
streams, and bounded draws with no bias. Not for cryptography.

Periods
: xorshift32 2^32 - 1, with weak low bits; xoshiro128** 2^128 - 1.

Seeds
: Any seed. xorshift32 maps 0 to 2463534242.

Bounded draws
: Uniform in [0, n) with no bias, by Lemire's method.

Streams
: `fu_xoshiro128_jump` advances 2^64 steps; jumped copies of one seeded
  generator do not overlap.

Asserts
: `n` is 0.

## Numbers

| function | cycles | newlib | ratio |
| --- | --: | --: | --: |
| `fu_xorshift32` | 8 | | |
| `fu_xoshiro128ss` | 22 | 44 | 2.00 |
| `fu_xoshiro128_below`, n = 100 | 33 | | |
| `fu_xoshiro128_jump` | 3268 | | |

Medians on an STM32F103 at 72 MHz, `-O2`, inlined. newlib: `rand()`, 31
bits per call. A plain `% 100` on xoshiro128** takes 38 and is biased.

## Example

```{literalinclude} ../examples/fu_rand.c
:language: c
```

## API

### xorshift32

```{doxygenstruct} FU_Xorshift32
```

```{doxygenfunction} fu_xorshift32_seed
```

```{doxygenfunction} fu_xorshift32
```

```{doxygenfunction} fu_xorshift32_below
```

### xoshiro128**

```{doxygenstruct} FU_Xoshiro128
```

```{doxygenfunction} fu_xoshiro128_seed
```

```{doxygenfunction} fu_xoshiro128ss
```

```{doxygenfunction} fu_xoshiro128_below
```

```{doxygenfunction} fu_xoshiro128_jump
```

## Verification

- Known-answer sequences from the reference code for both generators and
  for the jump.
- Chi-square tests of the bounded draws.
