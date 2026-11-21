# flexutils

flexutils holds the small pieces most firmware ends up writing for itself:
taking bytes apart, turning numbers into text and back without printf,
checking frames with a CRC, passing bytes from an interrupt to the main loop,
framing packets on a serial link, debouncing buttons and making random test
data. Each module is a header and, for most, one `.c` file. Nothing
allocates memory, and nothing depends on the C library beyond `<stdint.h>`
and friends.

| header | what it is for |
|---|---|
| [`fu_bits.h`](fu_bits.md) | counting and reversing bits, reading and writing either byte order at any address, sign extension |
| [`fu_fmt.h`](fu_fmt.md) | integers, hex, Q16.16 and floats to text, and a small `snprintf` |
| [`fu_parse.h`](fu_parse.md) | text to integers, hex and Q16.16, with a status instead of `errno` |
| [`fu_crc.h`](fu_crc.md) | CRC-8, CRC-16 and CRC-32, fed in pieces if you like |
| [`fu_ring.h`](fu_ring.md) | a lock-free byte ring and element queue between an interrupt and the main loop, and a DMA drain |
| [`fu_cobs.h`](fu_cobs.md) | COBS framing for serial links, with a byte-at-a-time receiver |
| [`fu_debounce.h`](fu_debounce.md) | debouncing one input, or 32 at once |
| [`fu_rand.h`](fu_rand.md) | xorshift32 and xoshiro128** generators, and draws in a range without bias |

To use only flexutils, set `FLEXLIB_LIBS := utils`.
[Get started](../get-started.md) shows the rest of the setup.

## Options

`FU_CRC_SMALL`
: Use 16-entry CRC tables instead of byte tables. They take far less flash,
  and they are faster when the tables are not in the flash cache.
  [CRCs](fu_crc.md) has the numbers.

`FX_ASSERT_FAIL(file, line)`, `FX_DISABLE_ASSERT`
: By default a failed argument check traps. Define the first to report it
  your own way, or the second to remove the checks.

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
