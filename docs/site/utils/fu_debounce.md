<p class="fl-header-name">fu_debounce.h</p>

# Debouncing

Debouncing for switches and buttons: an integrator for one input, and a
vertical counter that debounces 32 inputs in one call.

Ticks
: Call the update once per tick with the raw sample.

One input
: A settled input switches after `samples` equal samples in a row; a
  shorter glitch only moves the count.

32 inputs
: A bit flips after 4 samples in a row that differ from it.

Asserts
: `samples` is 0.

## Numbers

| function | cycles |
|---|--:|
| `fu_debounce32`, 32 inputs | 20 |
| `fu_debounce`, 1 input | 18 |

Medians per tick on an STM32F103 at 72 MHz, `-O2`, inlined. 16 inputs
through `fu_debounce` take 379.

## Example

```{literalinclude} ../examples/fu_debounce.c
:language: c
```

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

## Verification

- Both against reference counters on inputs that bounce after each change.
- A glitch shorter than `samples` never changes a settled input.
