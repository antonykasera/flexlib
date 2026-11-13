<p class="fl-header-name">fu_ring.h</p>

# Rings

A lock-free byte ring for one producer and one consumer, a queue of
fixed-size elements with the same rules, and a drain that moves a circular
DMA receive buffer into a ring.

Concurrency
: One producer and one consumer on one core; either may be an interrupt.
  Only compiler barriers order the accesses.

Sides
: The producer calls `put`, `write`, `from_dma` and `push`; the consumer
  `get`, `read`, `peek`, `skip`, `span` and `pop`. The counts work from
  either side.

Sizes
: A power of two, up to 2^31 bytes or elements.

Asserts
: A size that is not a power of two, `skip` past the bytes waiting, a NULL
  buffer with a nonzero count.

:::{warning}
`fu_ring_from_dma` cannot see a DMA that has lapped `*dma_pos`. Call it
before the DMA writes a full buffer more, or bytes are lost without notice.
:::

Checked
: - Random sequences of every operation against a plain array model, for the
    ring and the queue.
  - A simulated circular DMA writing a byte stream, with wraps and a full ring.

## API

### Byte ring

```{doxygenstruct} FU_Ring
```

```{doxygenfunction} fu_ring_init
```

```{doxygenfunction} fu_ring_used
```

```{doxygenfunction} fu_ring_free
```

```{doxygenfunction} fu_ring_put
```

```{doxygenfunction} fu_ring_get
```

```{doxygenfunction} fu_ring_write
```

```{doxygenfunction} fu_ring_read
```

```{doxygenfunction} fu_ring_peek
```

```{doxygenfunction} fu_ring_skip
```

```{doxygenfunction} fu_ring_span
```

```{doxygenfunction} fu_ring_from_dma
```

### Queue

```{doxygenstruct} FU_Queue
```

```{doxygenfunction} fu_queue_init
```

```{doxygenfunction} fu_queue_push
```

```{doxygenfunction} fu_queue_pop
```

```{doxygenfunction} fu_queue_used
```

## Numbers

| function | cycles | newlib | ratio |
|---|--:|--:|--:|
| `fu_ring_write` + `fu_ring_read`, 64 B | {cy}`fu_ring write + read, 64 B` | {cy}`ring, newlib memcpy, 64 B` |  |
| `fu_ring_write` + `fu_ring_read`, 8 B | {cy}`fu_ring write + read, 8 B` | {cy}`ring, newlib memcpy, 8 B` |  |
| `fu_ring_put` + `fu_ring_get` | {cy}`fu_ring_put + fu_ring_get (inline)` |  |  |
| `fu_queue_push` + `fu_queue_pop`, 12 B | {cy}`fu_queue push + pop, 12 B` |  |  |
| `fu_ring_from_dma` + `fu_ring_read`, 64 B | {cy}`fu_ring_from_dma + read, 64 B` |  |  |

Medians on an STM32F103 at 72 MHz, `-O2`. newlib: the same ring copying
with newlib-nano's `memcpy`, which moves one byte at a time.

## Example

```{literalinclude} ../examples/fu_ring.c
:language: c
```
