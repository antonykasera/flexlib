<p class="fl-header-name">fu_bits.h</p>

# Bits and bytes

Bit counts, bit and byte reversal, loads and stores of either byte order at
any alignment, and sign extension of N-bit fields. Every function is inline.

Counts
: A zero input gives the full width: 32, or 64 for `fu_clz64` and
  `fu_ctz64`.

Alignment
: Loads and stores take any address.

Asserts
: `fu_sext` with `bits` outside 1 to 32.

Checked
: - Counts at 0, all ones and every single bit, then random inputs, against
    plain loops.
  - Loads and stores at offsets 0 to 7 against byte-by-byte references.
  - `fu_sext` on every 12-bit value, then 100000 random values at random
    widths from 1 to 32, against a reference.

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

## Example

```{literalinclude} ../examples/fu_bits.c
:language: c
```
