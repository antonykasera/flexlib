<p class="fl-header-name">fm_float.h</p>

# Float functions

Code written with `float` still has to run on parts without an FPU, and
there newlib's `sinf` takes {cy}`sinf` cycles and `sqrtf`
{cy}`sqrtf`. The functions in `fm_float.h` take and return `float` as
usual, but compute with integer operations inside. The roots and reciprocals
are correctly rounded, and the rest are within about half a ULP. The numbers
below compare their cycles with newlib's.

## Polar form, and a large angle

```{literalinclude} ../examples/fm_float.c
:language: c
```

Which prints

```{literalinclude} ../examples/fm_float.out
:language: text
```

The length of (3, 4) comes out exactly 5, and its angle is 0.9272952 rad.
The sine of 10^6 rad matches `double`'s to every printed digit. A large
argument is the hard case: reducing it to one turn needs many more bits than
a float has, and newlib's `sinf` takes {cy}`sinf, |x| pi/4 .. 2^22` cycles
for arguments from $pi/4$ to 2^22.
{c:func}`fm_sinf` reduces any finite argument exactly, so 10^6 rad costs
the same as 1.

## Accuracy

{c:func}`fm_sqrtf`, {c:func}`fm_recipf` and {c:func}`fm_rsqrtf` are
correctly rounded, subnormals included: the result is the float nearest the
true value. {c:func}`fm_sinf` and {c:func}`fm_cosf` are within 0.515 ULP for
any finite argument. {c:func}`fm_atan2f`, {c:func}`fm_asinf` and
{c:func}`fm_acosf` are within 0.545 ULP. The logarithms and exponentials are
within 0.52 ULP; an exponential too large gives +inf, and one too small +0.

## Special values

They follow C's rules. A NaN input comes back quieted, with its payload. A
domain error gives the default NaN: $x < 0$ for the roots and logarithms,
$|x| > 1$ for asin and acos, an infinite angle for sin and cos. ±0 gives
±inf for the reciprocals and -inf for the logarithms, and
{c:func}`fm_atan2f` follows `atan2f` on zeros and infinities.

## How it was checked

- `make exhaustive`: every function on all 2^32 floats (atan2: 2^32 hashed
  pairs) against `double`. The roots and reciprocals match bit for bit, and
  {c:func}`fm_sincosf` gives the bits of {c:func}`fm_sinf` and
  {c:func}`fm_cosf`.
- Host tests: the special values, signed zeros, subnormals, results that
  round up to a power of two, and atan2 across exponent gaps from -40 to
  40.

## Numbers

Medians on an STM32F103 at 72 MHz, `-O2`, warm cache. The newlib column is
newlib-nano's libm, or libgcc's float division where shown. Max errors are
the exhaustive run's.

| function | cycles | newlib | max error, ULP |
|---|--:|--:|--:|
| `fm_sqrtf` | {cy}`fm_sqrtf` | {cy}`sqrtf, same inputs` | correctly rounded |
| `fm_recipf` | {cy}`fm_recipf` | `1.0f / x` {cy}`1.0f / x` | correctly rounded |
| `fm_rsqrtf` | {cy}`fm_rsqrtf` | `1.0f / sqrtf(x)` {cy}`1.0f / sqrtf(x)` | correctly rounded |
| `fm_sinf` | {cy}`fm_sinf` | {cy}`sinf` | 0.5092 |
| `fm_cosf` | {cy}`fm_cosf` | {cy}`cosf` | 0.5135 |
| `fm_sincosf` | {cy}`fm_sincosf` |  | 0.5135 |
| `fm_atan2f` | {cy}`fm_atan2f` | {cy}`atan2f` | 0.5385 |
| `fm_asinf` | {cy}`fm_asinf` | {cy}`asinf` | 0.5412 |
| `fm_acosf` | {cy}`fm_acosf` | {cy}`acosf` | 0.5407 |
| `fm_log2f` | {cy}`fm_log2f` | {cy}`log2f, same inputs` | 0.5061 |
| `fm_logf` | {cy}`fm_logf` | {cy}`logf, same inputs` | 0.5078 |
| `fm_exp2f` | {cy}`fm_exp2f` | {cy}`exp2f, same inputs` | 0.5026 |
| `fm_expf` | {cy}`fm_expf` | {cy}`expf, same inputs` | 0.5028 |

For $pi/4 <= |x| <= 2^22$, {c:func}`fm_sinf` takes {cy}`fm_sinf, |x| pi/4 .. 2^22` cycles and
newlib's `sinf` {cy}`sinf, |x| pi/4 .. 2^22`. CMSIS-DSP's `arm_sin_f32` takes {cy}`arm_sin_f32` cycles and reaches
{acc}`sinf; CMSIS-DSP; 10^7 random, x in [-pi, pi]` ULP on $[-pi, pi]$; `arm_atan2_f32` takes {cy}`arm_atan2_f32` and reaches
{acc}`atan2f; CMSIS-DSP; 10^7 random pairs in [-1, 1]` ULP. With the flash cache flushed before each call, {c:func}`fm_sinf`
takes {cy}`fm_sinf @ cold` cycles and newlib's `sinf`
{cy}`sinf @ cold`.

## API

### Roots and reciprocals

```{doxygenfunction} fm_sqrtf
```

```{doxygenfunction} fm_recipf
```

```{doxygenfunction} fm_rsqrtf
```

### Trigonometry

```{doxygenfunction} fm_sinf
```

```{doxygenfunction} fm_cosf
```

```{doxygenfunction} fm_sincosf
```

```{doxygenfunction} fm_atan2f
```

```{doxygenfunction} fm_asinf
```

```{doxygenfunction} fm_acosf
```

### Logarithms and exponentials

```{doxygenfunction} fm_log2f
```

```{doxygenfunction} fm_logf
```

```{doxygenfunction} fm_exp2f
```

```{doxygenfunction} fm_expf
```
