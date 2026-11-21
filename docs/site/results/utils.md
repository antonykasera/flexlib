# flexutils results

Every flexutils case and its baselines, by module, in cycles.
[Results](index.md) gives the conditions.

## Bits and bytes

[`fu_bits.h`](../utils/fu_bits.md)

:::{bench-rows} fu_bits.h
popcount __builtin
popcount SWAR
:::

## Formatting

[`fu_fmt.h`](../utils/fu_fmt.md)

:::{bench-rows} fu_fmt.h
snprintf
:::

## Parsing

[`fu_parse.h`](../utils/fu_parse.md)

:::{bench-rows} fu_parse.h
strtoul
strtol,
strtod
:::

## CRCs

[`fu_crc.h`](../utils/fu_crc.md)

:::{bench-rows} fu_crc.h
crc32 F1 CRC unit
:::

## Rings

[`fu_ring.h`](../utils/fu_ring.md)

:::{bench-rows} fu_ring.h
fu_ring write
fu_queue push
:::

## COBS framing

[`fu_cobs.h`](../utils/fu_cobs.md)

:::{bench-rows} fu_cobs.h
:::

## Debouncing

[`fu_debounce.h`](../utils/fu_debounce.md)

:::{bench-rows} fu_debounce.h
:::

## Random numbers

[`fu_rand.h`](../utils/fu_rand.md)

:::{bench-rows} fu_rand.h
rand()
:::
