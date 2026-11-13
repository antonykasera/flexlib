<p class="fl-header-name">fu_cobs.h</p>

# COBS framing

Consistent Overhead Byte Stuffing: an encoded frame holds no 0 byte, so a 0
on a serial link marks the end of each frame. Block encode and decode, and a
receiver fed one byte at a time.

Overhead
: At most one byte per 254, plus one: `FU_COBS_MAX(n)`.

Delimiter
: `fu_cobs_encode` does not write the ending 0; `fu_cobs_decode` takes the
  frame without it.

In place
: `fu_cobs_decode` may write over its input.

Errors
: `fu_cobs_decode` returns false on malformed input. The receiver drops bad
  and oversize frames and counts them in `errors`; empty frames are skipped.

Asserts
: A NULL buffer with a nonzero length.

## Numbers

| function | cycles | byte loop | ratio |
|---|--:|--:|--:|
| `fu_cobs_encode`, 64 B random | 329 | 996 | 3.03 |
| `fu_cobs_encode`, 256 B random | 1234 | 3886 | 3.15 |
| `fu_cobs_encode`, 64 B, 1/4 zeros | 858 | 1115 | 1.30 |
| `fu_cobs_decode`, 64 B random | 342 | 906 | 2.65 |
| `fu_cobs_decode`, 64 B, 1/4 zeros | 981 | 1177 | 1.20 |
| `fu_cobs_rx_byte`, 64 B frame and its 0 | 2217 | | |

Medians on an STM32F103 at 72 MHz, `-O2`. Byte loop: the usual
one-byte-at-a-time code. Random data has few zeros; a quarter zeros is the
slow case.

## Example

```{literalinclude} ../examples/fu_cobs.c
:language: c
```

## API

```{doxygendefine} FU_COBS_MAX
```

```{doxygenfunction} fu_cobs_encode
```

```{doxygenfunction} fu_cobs_decode
```

```{doxygenstruct} FU_CobsRx
:members:
```

```{doxygenfunction} fu_cobs_rx_init
```

```{doxygenfunction} fu_cobs_rx_byte
```

## Verification

- The examples on Wikipedia's COBS page.
- Random frames round-trip, decoded in place and into a second buffer.
- Malformed input: a 0 code, a 0 in the data, a block past the end.
- The receiver against the block decoder on frames cut short.
