<p class="fl-header-name">fu_fmt.h</p>

# Formatting

Numbers to text without printf: decimal, hex, Q16.16 and float, and
`fu_snprintf`, a small snprintf with a `%q` conversion for Q16.16.

Exactness
: `fu_fmt_q16` and `fu_fmt_f32` print the same digits as printf's `%.*f`,
  rounding the exact value, ties to even. NaN prints `nan`.

Buffers
: Each `fu_fmt_` function writes the text and a NUL into a buffer of its
  `_SIZE` and returns the length without the NUL.

Truncation
: `fu_snprintf` cuts the text to `size` - 1 bytes, ends it with a NUL and
  returns the full length, as snprintf does.

Asserts
: More than 9 fraction digits, `min_digits` above 8, an unknown conversion.

Checked
: - `fu_fmt_u32` and `fu_fmt_hex32` on all 2^32 inputs against a plain
    reference.
  - `fu_fmt_q16` on all 2^32 values and `fu_fmt_f32` on all 2^32 bit patterns
    against glibc's `%.*f`, every fraction count from 0 to 9.
  - `fu_snprintf` at every buffer size from 0 to the length + 2: the right
    prefix, a NUL, no write past `size`.

## API

```{doxygenfunction} fu_fmt_u32
```

```{doxygendefine} FU_FMT_U32_SIZE
```

```{doxygenfunction} fu_fmt_i32
```

```{doxygendefine} FU_FMT_I32_SIZE
```

```{doxygenfunction} fu_fmt_hex32
```

```{doxygendefine} FU_FMT_HEX32_SIZE
```

```{doxygenfunction} fu_fmt_q16
```

```{doxygendefine} FU_FMT_Q16_SIZE
```

```{doxygenfunction} fu_fmt_f32
```

```{doxygendefine} FU_FMT_F32_SIZE
```

```{doxygendefine} FU_FMT_FRAC_MAX
```

```{doxygenfunction} fu_fmt_dec_digits
```

```{doxygenfunction} fu_snprintf
```

```{doxygenfunction} fu_vsnprintf
```

## Numbers

| function | cycles | newlib | ratio |
| --- | --: | --: | --: |
| `fu_fmt_u32`, 10 digits | {cy}`fu_fmt_u32, 10 digits` | {cy}`snprintf %lu, 10 digits` |  |
| `fu_fmt_u32`, log | {cy}`fu_fmt_u32, log` | {cy}`snprintf %lu, log` |  |
| `fu_fmt_i32`, log | {cy}`fu_fmt_i32, log` |  |  |
| `fu_fmt_hex32`, 8 digits | {cy}`fu_fmt_hex32, 8 digits` |  |  |
| `fu_fmt_q16`, 4 digits, log | {cy}`fu_fmt_q16, %.4f, log` | {cy}`snprintf %.4f of q16, log` |  |
| `fu_fmt_f32`, 2 digits | {cy}`fu_fmt_f32, %.2f, 2^-7..2^13` | {cy}`snprintf %.2f, 2^-7..2^13` |  |
| `fu_snprintf`, u32 + i32 + hex | {cy}`fu_snprintf u32 + i32 + hex line` | {cy}`snprintf u32 + i32 + hex line` |  |

Medians on an STM32F103 at 72 MHz, `-O2`. newlib: `snprintf` with the
matching format. log: inputs spread evenly on a log scale; floats between
2^-7 and 2^13.

## Example

```{literalinclude} ../examples/fu_fmt.c
:language: c
```
