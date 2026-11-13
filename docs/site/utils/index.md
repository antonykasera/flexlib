# flexutils

Common use Cortex-M code. Every input that can be taken has been validated: bit tricks,
efficient formatting and parsing without printf, CRCs, a lock-free ring, COBS,
debouncing and PRNGs. C11 with no use of allocations.

## Modules

::::{grid} 1 2 3 3
:gutter: 3

:::{grid-item-card} Bits and bytes
:link: fu_bits
:link-type: doc

`fu_bits.h` · counts, reversal, endian loads, sign extension

```{card-footer}
popcount: 13 cycles
```

:::

:::{grid-item-card} Formatting
:link: fu_fmt
:link-type: doc

`fu_fmt.h` · decimal, hex, Q16.16, float, `fu_snprintf`

```{card-footer}
u32: 116 cycles, newlib 1024
```

:::

:::{grid-item-card} Parsing
:link: fu_parse
:link-type: doc

`fu_parse.h` · u32, i32, hex, Q16.16 with a status

```{card-footer}
Q16.16: 275 cycles, newlib 1332
```

:::

:::{grid-item-card} CRCs
:link: fu_crc
:link-type: doc

`fu_crc.h` · CRC-8, CRC-16, CRC-32

```{card-footer}
CRC-32: 1584 cycles for 256 B
```

:::

:::{grid-item-card} Rings
:link: fu_ring
:link-type: doc

`fu_ring.h` · SPSC byte ring, queue, DMA receive

```{card-footer}
64 B in and out: 392 cycles
```

:::

:::{grid-item-card} COBS framing
:link: fu_cobs
:link-type: doc

`fu_cobs.h` · encode, decode, byte receiver

```{card-footer}
encode 64 B: 329 cycles
```

:::

:::{grid-item-card} Debouncing
:link: fu_debounce
:link-type: doc

`fu_debounce.h` · one input, or 32 at once

```{card-footer}
32 inputs: 20 cycles per tick
```

:::

:::{grid-item-card} Random numbers
:link: fu_rand
:link-type: doc

`fu_rand.h` · xorshift32, xoshiro128**, unbiased bounds

```{card-footer}
xoshiro128**: 22 cycles
```

:::

::::

## Options

`FU_CRC_SMALL`
: 16-entry CRC tables instead of byte tables.

`FX_ASSERT_FAIL(file, line)`, `FX_DISABLE_ASSERT`
: Argument checks trap by default.

```{toctree}
:hidden:

fu_bits
fu_fmt
fu_parse
fu_crc
fu_ring
fu_cobs
fu_debounce
fu_rand
```
