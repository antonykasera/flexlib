<p class="fl-header-name">fu_ring.h</p>

# Rings

An interrupt receives bytes from a UART or USB, and the main loop deals with
them later. Something has to sit between the two that the interrupt can
write while the main loop reads, without disabling interrupts. `fu_ring.h`
has that: a byte ring for one producer and one consumer, a queue of
fixed-size elements with the same rules, and a function that drains a
circular DMA receive buffer into a ring.

## Draining a DMA buffer

Here a DMA channel writes received bytes into `dma[]` in circular mode. The
main loop moves what has arrived into the ring, then reads it:

```{literalinclude} ../examples/fu_ring.c
:language: c
```

Which prints

```{literalinclude} ../examples/fu_ring.out
:language: text
```

The DMA's count register reads 11 out of 16, so it has written 5 bytes.
{c:func}`fu_ring_from_dma` copies the bytes between `pos` and that point
into the ring and moves `pos` to 5, ready for the next call.
{c:func}`fu_ring_read` then takes `hello` out, and the ring is empty again.

## Who may call what

There is one producer and one consumer, on one core, and either may be an
interrupt. The producer calls `put`, `write`, `from_dma` and `push`. The
consumer calls `get`, `read`, `peek`, `skip`, `span` and `pop`. The counts
of used and free space work from either side. Nothing else is needed: the
two sides never write the same index, and compiler barriers keep the
accesses in order. With two producers, or with the two sides on different
cores, you need a lock of your own.

The size of the ring or queue must be a power of two, up to 2^31 bytes or
elements.

:::{warning}
{c:func}`fu_ring_from_dma` cannot tell if the DMA has gone all the way
round past `*dma_pos` since the last call. Call it before the DMA writes a
whole buffer more, or bytes are lost without notice. Calling it from the
DMA's half-transfer and transfer-complete interrupts is enough to rule that
out.
:::

`skip` past the bytes waiting, a size that is not a power of two, and a NULL
buffer with a nonzero count stop at an assert.

## How it was checked

- Random sequences of every operation against a plain array model, for the
  ring and the queue.
- A simulated circular DMA writing a byte stream, with wraps and a full
  ring.

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
