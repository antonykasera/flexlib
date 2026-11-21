<p class="fl-header-name">fu_bits.h</p>

# Bits and bytes

A lot of firmware is taking bytes apart: a sensor frame that arrives
big-endian, a 12-bit signed field packed into two bytes, a status word whose
set bits you need to count. `fu_bits.h` has these small operations. They are
all inline, and most compile to one or two instructions.

## Reading a sensor frame

Here a frame holds a 16-bit id and then a 12-bit signed reading, both
big-endian:

```{literalinclude} ../examples/fu_bits.c
:language: c
```

Which prints

```{literalinclude} ../examples/fu_bits.out
:language: text
```

{c:func}`fu_load_be16` reads the first two bytes as `0x1234`, whatever the
host's byte order. The reading is `0xF9C`, and bit 11, the sign bit of a
12-bit field, is set. {c:func}`fu_sext` copies that bit into the upper bits
and gives -100. {c:func}`fu_store_le32` writes `0xDEADBEEF` lowest byte
first, so `out[0]` is `0xEF`, and `0xF0F0` has 8 bits set.

## Counting and reversing bits

{c:func}`fu_clz` and {c:func}`fu_ctz` count the leading and trailing zero
bits. A zero input gives the full width, 32, so you do not need a special
case for it. The 64-bit forms {c:func}`fu_clz64` and {c:func}`fu_ctz64` give
64 for zero. {c:func}`fu_popcount` counts the set bits, and
{c:func}`fu_is_pow2` tells you whether exactly one bit is set.

On the Cortex-M3 `fu_clz`, `fu_ctz`, `fu_rbit` and `fu_bswap32` are one or
two instructions each. Elsewhere they fall back to plain C with the same
results.

## Byte order and alignment

The `fu_load_` and `fu_store_` functions read and write 16- and 32-bit
values in little- or big-endian order at any address. You can point them at
any byte of a receive buffer, aligned or not, and they never fault. An
unaligned load costs one or two cycles more than an aligned one.

## Sign extension

{c:func}`fu_sext` treats the low `bits` bits of a value as a two's complement
number and returns it as `int32_t`. `bits` goes from 1 to 32; anything else
stops at an assert. With a constant width it compiles to a single SBFX
instruction.

## How it was checked

- The counts at 0, all ones and every single bit, then on random inputs,
  against plain loops.
- The loads and stores at address offsets 0 to 7, against byte-by-byte
  references.
- {c:func}`fu_sext` on every 12-bit value, then on 100000 random values at
  random widths from 1 to 32.

## Numbers

| function | cycles | libgcc | ratio |
|---|--:|--:|--:|
| `fu_popcount` | {cy}`fu_popcount` | {cy}`popcount __builtin` |  |
| `fu_clz64`, high half 0 | {cy}`fu_clz64, half hi = 0` |  |  |
| `fu_clz64`, high half not 0 | {cy}`fu_clz64, hi != 0` |  |  |
| `fu_ctz64`, low half 0 | {cy}`fu_ctz64, half lo = 0` |  |  |
| `fu_load_be32`, aligned | {cy}`fu_load_be32, aligned` |  |  |
| `fu_load_be32`, address + 1 | {cy}`fu_load_be32, +1` |  |  |
| `fu_sext`, constant width | {cy}`fu_sext(x, 12)` |  |  |
| `fu_sext`, variable width | {cy}`fu_sext(x, n)` |  |  |

Medians on an STM32F103 at 72 MHz, `-O2`, inlined. libgcc:
`__builtin_popcount`. `fu_clz`, `fu_ctz`, `fu_rbit` and `fu_bswap32` are one
instruction each.

## API

### Counts

```{doxygenfunction} fu_clz
```

```{doxygenfunction} fu_ctz
```

```{doxygenfunction} fu_clz64
```

```{doxygenfunction} fu_ctz64
```

```{doxygenfunction} fu_popcount
```

```{doxygenfunction} fu_is_pow2
```

### Reversal

```{doxygenfunction} fu_rbit
```

```{doxygenfunction} fu_bswap32
```

```{doxygenfunction} fu_bswap16
```

### Loads and stores

```{doxygenfunction} fu_load_le16
```

```{doxygenfunction} fu_load_le32
```

```{doxygenfunction} fu_load_be16
```

```{doxygenfunction} fu_load_be32
```

```{doxygenfunction} fu_store_le16
```

```{doxygenfunction} fu_store_le32
```

```{doxygenfunction} fu_store_be16
```

```{doxygenfunction} fu_store_be32
```

### Sign extension

```{doxygenfunction} fu_sext
```
