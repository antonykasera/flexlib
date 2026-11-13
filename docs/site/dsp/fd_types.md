<p class="fl-header-name">fd_types.h</p>

# Types

Complex sample types, the status codes of the init and design functions,
and the band and window choices of the design functions.

Complex samples
: Interleaved `{re, im}` pairs, the CMSIS-DSP layout. A `fd_cq15` is one
  aligned word.

Status
: Init and design functions return `FD_Status` or assert;
  processing functions return nothing.

## API

```{doxygenstruct} fd_cq15
:members:
```

```{doxygenstruct} fd_cq31
:members:
```

```{doxygenstruct} fd_cf32
:members:
```

```{doxygenenum} FD_Status
```

```{doxygenenum} FD_Band
```

```{doxygenenum} FD_Window
```
