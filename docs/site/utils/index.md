# flexutils

Small building blocks for Cortex-M firmware: bit operations, number
formatting and parsing without printf, CRCs, a lock-free ring, COBS framing,
debouncing and PRNGs. C11, no allocation.

## Modules

| header | contents |
|---|---|
| [`fu_bits.h`](fu_bits.md) | counts, reversal, endian loads and stores, sign extension |
| [`fu_fmt.h`](fu_fmt.md) | decimal, hex, Q16.16 and float to text, `fu_snprintf` |
| [`fu_parse.h`](fu_parse.md) | text to u32, i32, hex and Q16.16, with a status |
| [`fu_crc.h`](fu_crc.md) | CRC-8, CRC-16, CRC-32 |
| [`fu_ring.h`](fu_ring.md) | single-producer single-consumer byte ring and queue, DMA receive |
| [`fu_cobs.h`](fu_cobs.md) | COBS encode, decode, byte-at-a-time receiver |
| [`fu_debounce.h`](fu_debounce.md) | one input, or 32 at once |
| [`fu_rand.h`](fu_rand.md) | xorshift32, xoshiro128**, unbiased bounds |

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
