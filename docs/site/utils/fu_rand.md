<p class="fl-header-name">fu_rand.h</p>

# Random numbers

Simulations, test data and timing jitter need numbers that look random but
repeat when you use the same seed. `fu_rand.h` has two generators:
xorshift32, the smallest and fastest, and xoshiro128**, which has better
statistics and can split into independent streams. Neither is suitable for
cryptography.

## Two streams and a die

```{literalinclude} ../examples/fu_rand.c
:language: c
```

Which prints

```{literalinclude} ../examples/fu_rand.out
:language: text
```

`a` is seeded with 42, and `b` starts as a copy of it.
{c:func}`fu_xoshiro128_jump` moves `b` ahead by 2^64 steps, which makes it a
second stream that will not overlap `a` in any realistic run.
{c:func}`fu_xoshiro128_below` then draws a number from 0 to 5 with no bias,
and the example adds 1 to roll a die.

## Which generator

xorshift32 has a period of 2^32 - 1 and weak low bits. Use it when you need
speed and do not look closely at the numbers, such as jitter or filler
data. xoshiro128** has a period of 2^128 - 1 and passes the usual
statistical tests. Use it for simulations and for anything that splits into
streams. Any seed works. xorshift32 cannot run from 0, so it maps a seed of
0 to 2463534242.

## Draws in a range

Taking `x % 6` of a 32-bit random number is slightly biased: 2^32 is not a
multiple of 6, so some results come up once more often than others. The
`_below` functions use Lemire's method, which removes the bias and needs a
division only in rare cases. `n` of 0 stops at an assert.

## How it was checked

- Known-answer sequences from the reference code, for both generators and
  for the jump.
- Chi-square tests of the draws in a range.

## Numbers

| function | cycles | newlib | ratio |
| --- | --: | --: | --: |
| `fu_xorshift32` | {cy}`fu_xorshift32 (inline)` |  |  |
| `fu_xoshiro128ss` | {cy}`fu_xoshiro128ss (inline)` | {cy}`rand()` |  |
| `fu_xoshiro128_below`, $n = 100$ | {cy}`fu_xoshiro128_below(100), unbiased` |  |  |
| `fu_xoshiro128_jump` | {cy}`fu_xoshiro128_jump (2^64 steps)` |  |  |

Medians on an STM32F103 at 72 MHz, `-O2`, inlined. newlib: `rand()`, 31
bits per call. A plain `% 100` on xoshiro128** takes {cy}`xoshiro128** % 100 (UDIV, biased)` and is biased.

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
