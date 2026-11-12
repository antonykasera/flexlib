<p class="fl-header-name">fu_crc.h</p>

# CRCs

Five table-driven CRCs for frames and stored data: CRC-8/SMBUS,
CRC-8/MAXIM, CRC-16/CCITT-FALSE, CRC-16/MODBUS and CRC-32 as zlib computes
it. Each call takes the running CRC, so a message can arrive in pieces.

Formats
: Bytes at any alignment in; the CRC in an integer of its width out.

Streaming
: Pass the `_INIT` value with the first piece and the previous result with
  each later one. {c:func}`fu_crc32` applies zlib's start and final XOR
  inside, so its result is both the finished CRC and the value to continue
  from.

Asserts
: `data` is `NULL` while `len` is not 0.

## Numbers

| function | cycles | byte table | ratio |
|---|--:|--:|--:|
| `fu_crc32`, 256 B | 1584 | 2829 | 1.79 |
| `fu_crc32`, 8 B | 97 | 100 | 1.03 |
| `fu_crc16_ccitt`, 64 B | 568 | 717 | 1.26 |
| `fu_crc16_modbus`, 64 B | 523 | 780 | 1.49 |
| `fu_crc8_smbus`, 64 B | 453 | 525 | 1.16 |
| `fu_crc8_maxim`, 64 B | 453 | 525 | 1.16 |

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache. Byte table: one
lookup per byte, the usual C loop; a bitwise loop takes 13843 for CRC-32
over 256 B.

## Example

```{literalinclude} ../examples/fu_crc.c
:language: c
```

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

## How it works

The tables sit in flash, and only those of the CRCs a program calls are
linked: 4 KB for CRC-32 (four byte tables, one 32-bit load per four bytes),
512 B for each CRC-16, 256 B for each CRC-8. `FU_CRC_SMALL` swaps them for
16-entry tables of 64, 32 and 16 B. CRC-32 over 256 B then takes 4109
cycles warm, but 4672 against 14793 when the tables are not in the flash
cache, which makes it the faster choice for rare calls.

## Verification

- Each CRC gives its published check value for `"123456789"`; CRC-16/CCITT
  started from 0 gives the XMODEM value.
- 4000 random buffers of 0 to 299 bytes, at offsets 0 to 7, match a bitwise
  reference, with and without `FU_CRC_SMALL`.
- A 67-byte message split at every point gives the same CRC as one call.
- On the board, every bench row checks its result against a golden value.
