<p class="fl-header-name">fu_cobs.h</p>

# COBS framing

On a serial link the receiver needs to know where one frame ends and the
next begins, even after it starts listening halfway through. Consistent
Overhead Byte Stuffing rewrites a frame so that it contains no 0 byte. A
single 0 can then mark the end of each frame, and a receiver that loses
sync finds the next frame at the next 0. `fu_cobs.h` encodes and decodes
whole frames, and has a receiver you feed one byte at a time.

## Sending and receiving a frame

The message here contains a 0 byte itself:

```{literalinclude} ../examples/fu_cobs.c
:language: c
```

Which prints

```{literalinclude} ../examples/fu_cobs.out
:language: text
```

{c:func}`fu_cobs_encode` turns the three bytes into four, none of them 0,
and the sender adds the 0 that ends the frame. On the other side each byte
goes to {c:func}`fu_cobs_rx_byte` as it arrives. When the ending 0 comes in,
it returns true with the decoded frame in `buf`: three bytes, the 0 in the
middle restored.

## Size and buffers

COBS adds at most one byte per 254, plus one: `FU_COBS_MAX(n)` is the
largest encoded size for `n` bytes. {c:func}`fu_cobs_encode` does not write
the ending 0, and {c:func}`fu_cobs_decode` takes the frame without it.
`fu_cobs_decode` may decode in place, over its own input.

## Bad input

{c:func}`fu_cobs_decode` returns false on input that is not valid COBS. The
receiver drops bad frames and frames too large for its buffer, counts them
in `errors`, and carries on with the next frame. An empty frame, two 0
bytes in a row, is skipped. A NULL buffer with a nonzero length stops at an
assert.

## How it was checked

- The examples on Wikipedia's COBS page.
- Random frames encoded and decoded, in place and into a second buffer.
- Malformed input: a 0 code, a 0 in the data, a block that runs past the
  end.
- The receiver against the block decoder, on frames cut short.

## Numbers

| function | cycles | byte loop | ratio |
|---|--:|--:|--:|
| `fu_cobs_encode`, 64 B random | {cy}`fu_cobs_encode, 64 B random` | {cy}`cobs encode byte loop, 64 B random` |  |
| `fu_cobs_encode`, 256 B random | {cy}`fu_cobs_encode, 256 B random` | {cy}`cobs encode byte loop, 256 B random` |  |
| `fu_cobs_encode`, 64 B, 1/4 zeros | {cy}`fu_cobs_encode, 64 B 1/4 zeros` | {cy}`cobs encode byte loop, 64 B 1/4 zeros` |  |
| `fu_cobs_decode`, 64 B random | {cy}`fu_cobs_decode, 64 B random` | {cy}`cobs decode byte loop, 64 B random` |  |
| `fu_cobs_decode`, 64 B, 1/4 zeros | {cy}`fu_cobs_decode, 64 B 1/4 zeros` | {cy}`cobs decode byte loop, 64 B 1/4 zeros` |  |
| `fu_cobs_rx_byte`, 64 B frame and its 0 | {cy}`fu_cobs_rx_byte, 64 B frame + 0` |  |  |

Medians on an STM32F103 at 72 MHz, `-O2`. Byte loop: the usual
one-byte-at-a-time code. Random data has few zeros; a quarter zeros is the
slow case.

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
