<p class="fl-header-name">fu_crc.h</p>

# CRCs

A CRC lets the receiver of a frame, or the code reading back stored data,
check that the bytes have not changed. `fu_crc.h` has the five you are most
likely to meet: CRC-8/SMBUS, CRC-8/MAXIM, CRC-16/CCITT-FALSE, CRC-16/MODBUS,
and CRC-32 as zlib computes it. Each call takes the running CRC, so a
message can arrive in pieces.

## Sealing a Modbus frame

Modbus RTU appends a CRC-16 to each frame, low byte first. The receiver runs
the CRC over the frame and its CRC together, and gets 0 when nothing
changed:

```{literalinclude} ../examples/fu_crc.c
:language: c
```

Which prints

```{literalinclude} ../examples/fu_crc.out
:language: text
```

The six-byte request gets the CRC bytes `C5 CD`, and the check on the
receiving side comes out 0, so `ok` is 1. The CRC-32 is fed `12345` and then
`6789`, and gives `CBF43926`: the published check value of CRC-32 for
`123456789` in one piece.

## Messages in pieces

Pass the `_INIT` value with the first piece and the previous result with
each later one. The result is the same as one call over the whole message.
{c:func}`fu_crc32` applies zlib's starting and final XOR inside, so its
result is both the finished CRC and the value to continue from.

The data may sit at any address. A NULL pointer with a nonzero length stops
at an assert.

## How it was checked

- Each CRC gives its published check value for `"123456789"`. CRC-16/CCITT
  started from 0 gives the XMODEM value.
- 4000 random buffers of 0 to 299 bytes, at address offsets 0 to 7, match a
  bitwise reference, with and without `FU_CRC_SMALL`.
- A 67-byte message split at every point gives the same CRC as one call.
- On the board, every bench row checks its result against a golden value.

## Numbers

| function | cycles | byte table | ratio |
|---|--:|--:|--:|
| `fu_crc32`, 256 B | {cy}`fu_crc32, 256 B` | {cy}`crc32 byte table 1 KB, 256 B` |  |
| `fu_crc32`, 8 B | {cy}`fu_crc32, 8 B` | {cy}`crc32 byte table 1 KB, 8 B` |  |
| `fu_crc16_ccitt`, 64 B | {cy}`fu_crc16_ccitt, 64 B` | {cy}`crc16 ccitt byte table, 1 per step, 64 B` |  |
| `fu_crc16_modbus`, 64 B | {cy}`fu_crc16_modbus, 64 B` | {cy}`crc16 modbus byte table, 1 per step, 64 B` |  |
| `fu_crc8_smbus`, 64 B | {cy}`fu_crc8_smbus, 64 B` | {cy}`crc8 smbus byte table, 1 per step, 64 B` |  |
| `fu_crc8_maxim`, 64 B | {cy}`fu_crc8_maxim, 64 B` | {cy}`crc8 maxim byte table, 1 per step, 64 B` |  |

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache. Byte table: one
lookup per byte, the usual C loop; a bitwise loop takes {cy}`crc32 bitwise, 256 B` for CRC-32
over 256 B.

## How it works

The tables sit in flash, and only those of the CRCs a program calls are
linked: 4 KB for CRC-32 (four byte tables, one 32-bit load per four bytes),
512 B for each CRC-16, 256 B for each CRC-8. `FU_CRC_SMALL` swaps them for
16-entry tables of 64, 32 and 16 B. CRC-32 over 256 B then takes 4109
cycles warm, but 4672 against 14793 when the tables are not in the flash
cache, which makes it the faster choice for rare calls.

## API

```{doxygenfunction} fu_crc8_smbus
```

```{doxygendefine} FU_CRC8_SMBUS_INIT
```

```{doxygenfunction} fu_crc8_maxim
```

```{doxygendefine} FU_CRC8_MAXIM_INIT
```

```{doxygenfunction} fu_crc16_ccitt
```

```{doxygendefine} FU_CRC16_CCITT_INIT
```

```{doxygenfunction} fu_crc16_modbus
```

```{doxygendefine} FU_CRC16_MODBUS_INIT
```

```{doxygenfunction} fu_crc32
```

```{doxygendefine} FU_CRC32_INIT
```
