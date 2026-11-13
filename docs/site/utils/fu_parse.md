<p class="fl-header-name">fu_parse.h</p>

# Parsing

Text to numbers without libc: unsigned and signed decimal, hex and Q16.16,
from a pointer and a length rather than a NUL-terminated string. Error Statuses. A parse can stop at the end of the number.

Input
: `s[0, len)`; no NUL needed, no spaces skipped.

Prefix
: With `used` NULL, all of `s` must be the number. Otherwise the number may
  be a prefix, and `*used` says where it ended.

Rounding
: `fu_parse_q16` rounds to the nearest 2^-16, ties to even, from any number
  of digits.

Range
: A number too large saturates and returns `FU_PARSE_RANGE`.

Asserts
: `out` is NULL, or `s` is NULL while `len` is not 0.

Checked
: - Every 32-bit value printed by `fu_fmt` and read back: u32, i32, hex and
    Q16.16.
  - `fu_parse_q16` against an exact decimal reference, and on every exact
    midpoint between two Q16.16 values.
  - Edge cases: empty input, a sign alone, overflow by one, trailing text with
    and without `used`.

## API

```{doxygenenum} FU_ParseStatus
```

```{doxygenfunction} fu_parse_u32
```

```{doxygenfunction} fu_parse_i32
```

```{doxygenfunction} fu_parse_hex32
```

```{doxygenfunction} fu_parse_q16
```

## Numbers

| function | cycles | newlib | ratio |
| --- | --: | --: | --: |
| `fu_parse_u32`, 10 digits | 153 | 327 | 2.14 |
| `fu_parse_u32`, log | 111 | 217 | 1.95 |
| `fu_parse_i32`, log | 133 | 228 | 1.71 |
| `fu_parse_hex32`, 8 digits | 196 | 314 | 1.60 |
| `fu_parse_q16`, 4 decimals | 275 | 1332 | 4.84 |

Medians on an STM32F103 at 72 MHz, `-O2`. newlib: `strtoul`, `strtol`,
`strtoul` base 16; for Q16.16, `strtod`, then `* 65536` and `lrint`. log:
inputs spread evenly on a log scale.

## Example

```{literalinclude} ../examples/fu_parse.c
:language: c
```
