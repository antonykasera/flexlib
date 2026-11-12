# flexutils

Everyday Cortex-M code, checked on every input it can take: bit tricks,
formatting and parsing without printf, CRCs, a lock-free ring, COBS,
debouncing and PRNGs. Plain C11, no allocation.

## Modules

::::{grid} 1 2 3 3
:gutter: 3

:::{grid-item-card} CRCs
:link: fu_crc
:link-type: doc

`fu_crc.h` · CRC-8, CRC-16, CRC-32

```{card-footer}
CRC-32: 1584 cycles for 256 B
```
:::

::::

## Options

`FU_CRC_SMALL`
: 16-entry CRC tables instead of byte tables.

`FX_ASSERT_FAIL(file, line)`, `FX_DISABLE_ASSERT`
: Argument checks trap by default, with no stdio.

```{toctree}
:hidden:

fu_crc
```
