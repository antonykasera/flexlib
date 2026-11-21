<p class="fl-header-name">fd_types.h</p>

# Types

The types every flexdsp module shares: complex samples, the status codes
that init and design functions return, and the filter bands and windows you
pick when you design a filter.

## Complex samples

A complex sample is an interleaved `{re, im}` pair, the layout CMSIS-DSP
uses, so buffers can pass between the two libraries. `fd_cq15` holds two
`fm_q15` values in one aligned 32-bit word, `fd_cq31` two `fm_q31`, and
`fd_cf32` two floats. A complex FFT of n points works on n of them in place.

## Status codes

Init and design functions return an `FD_Status`: `FD_OK`, or a code that
says what went wrong at run time, such as a design whose coefficients do not
fit the format. Mistakes in the arguments themselves, a zero length or a
buffer too small, stop at an assert instead. Processing functions return
nothing, since there is nothing left to go wrong once the filter is set up.

## Bands and windows

`FD_Band` names the filter type for the design functions: lowpass,
highpass, bandpass or bandstop. `FD_Window` names a fixed window for an FIR
design or a spectrum: rectangular, Hann, Hamming, Blackman, Blackman-Harris,
Nuttall, flat-top or Bartlett. Kaiser, Gaussian and Tukey take a parameter
and have functions of their own in [](fd_window.md).

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
