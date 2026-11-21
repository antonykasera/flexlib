<p class="fl-header-name">fu_debounce.h</p>

# Debouncing

A mechanical switch bounces: for a few milliseconds after it closes, it
reads on and off before it settles. Read it directly and one press can look
like several. `fu_debounce.h` has two debouncers. One handles a single
input with a count you choose. The other handles 32 inputs, one bit each,
in a single call.

## A key that bounces

Here key 0 is read once per tick. It goes down, bounces once, then stays
down:

```{literalinclude} ../examples/fu_debounce.c
:language: c
```

Which prints

```{literalinclude} ../examples/fu_debounce.out
:language: text
```

{c:func}`fu_debounce32` flips a bit only after 4 samples in a row that
differ from its debounced state. The 1 at tick 0 starts a count, the bounce
at tick 1 resets it, and ticks 2 to 5 are four 1s in a row. The key reads as
pressed at tick 5, once.

## Ticks

Call the update once per tick with the raw sample, for example from a 1 ms
timer interrupt. The debounce time is the tick period times the number of
samples: with a 1 ms tick, the 32-input form needs 4 ms of steady input.

## One input or 32

{c:func}`fu_debounce` is an integrator. Each sample moves a count up or down
by one, and the input switches when the count reaches the top or 0. A
settled input therefore switches after `samples` equal samples in a row,
and a shorter glitch only moves the count. `samples` of 0 stops at an
assert.

{c:func}`fu_debounce32` is a vertical counter. It keeps a 2-bit count for
each of 32 inputs in two words and updates them all with a few logic
operations, so 32 buttons cost about as much as one. It returns the bits
that changed in this tick.

## How it was checked

- Both against reference counters, on inputs that bounce after each change.
- A glitch shorter than `samples` never changes a settled input.

## Numbers

| function | cycles |
|---|--:|
| `fu_debounce32`, 32 inputs | {cy}`fu_debounce32, 32 inputs per tick (inline)` |
| `fu_debounce`, 1 input | {cy}`fu_debounce, 1 input per tick (inline)` |

Medians per tick on an STM32F103 at 72 MHz, `-O2`, inlined. 16 inputs
through `fu_debounce` take {cy}`16 x fu_debounce, 16 inputs per tick`.

## API

```{doxygenstruct} FU_Debounce
:members:
```

```{doxygenfunction} fu_debounce_init
```

```{doxygenfunction} fu_debounce
```

```{doxygenstruct} FU_Debounce32
:members:
```

```{doxygenfunction} fu_debounce32_init
```

```{doxygenfunction} fu_debounce32
```
