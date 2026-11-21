<p class="fl-header-name">fu_parse.h</p>

# Parsing

Commands that arrive over a serial line are text: `12.5V`, `addr 4096`.
`strtod` and `strtoul` can read them, but they want a NUL-terminated string,
report errors through `errno` and, for `strtod`, take
{cy}`strtod * 65536 + lrint, 4 decimals` cycles in soft float. The
`fu_parse` functions read unsigned, signed, hex and Q16.16 numbers from a
pointer and a length, and return a status.

## Reading a command

```{literalinclude} ../examples/fu_parse.c
:language: c
```

Which prints

```{literalinclude} ../examples/fu_parse.out
:language: text
```

The first call reads `12.5` as Q16.16, which is 819200. Because the example
passes `used`, the number may be followed by other text: parsing stops at
`V` and `used` says 4 characters were read. The second call has no `used`,
so the whole string must be a number. It is, but it is one more than
`UINT32_MAX`. The result saturates to 4294967295 and the status is
`FU_PARSE_RANGE`, which prints as 2.

## Whole strings and prefixes

The text is `s[0]` to `s[len - 1]`. It needs no NUL, and spaces are not
skipped. If you pass `used` as NULL, all of it must be the number, and
anything else returns `FU_PARSE_SYNTAX`. If you pass a pointer, the number
may be a prefix, and `*used` tells you where it ended so you can go on with
the rest. On `FU_PARSE_SYNTAX` neither `out` nor `used` is changed.

## Rounding and range

{c:func}`fu_parse_q16` reads any number of decimal digits and rounds the
exact value to the nearest 2^-16, ties to even. Everything
{c:func}`fu_fmt_q16` prints reads back as the same value. A number too large
for its type saturates and returns `FU_PARSE_RANGE`, so you get both the
closest value and the fact that it did not fit.

## How it was checked

- Every 32-bit value printed by `fu_fmt` and read back: u32, i32, hex and
  Q16.16.
- {c:func}`fu_parse_q16` against an exact decimal reference, and on every
  exact midpoint between two Q16.16 values.
- The edges: empty input, a sign alone, overflow by one, and text after the
  number with and without `used`.

## Numbers

| function | cycles | newlib | ratio |
| --- | --: | --: | --: |
| `fu_parse_u32`, 10 digits | {cy}`fu_parse_u32, 10 digits` | {cy}`strtoul, 10 digits` |  |
| `fu_parse_u32`, log | {cy}`fu_parse_u32, log` | {cy}`strtoul, log` |  |
| `fu_parse_i32`, log | {cy}`fu_parse_i32, log` | {cy}`strtol, log` |  |
| `fu_parse_hex32`, 8 digits | {cy}`fu_parse_hex32, 8 digits` | {cy}`strtoul base 16, 8 digits` |  |
| `fu_parse_q16`, 4 decimals | {cy}`fu_parse_q16, 4 decimals` | {cy}`strtod * 65536 + lrint, 4 decimals` |  |

Medians on an STM32F103 at 72 MHz, `-O2`. newlib: `strtoul`, `strtol`,
`strtoul` base 16; for Q16.16, `strtod`, then `* 65536` and `lrint`. log:
inputs spread evenly on a log scale.

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
