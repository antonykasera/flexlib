# flexlib

Compute libraries for Cortex-M, measured on the chip they're for:

- **[flexutils](utils/README.md)** (`fu_`): bit tricks, rings, formatting and
  parsing, CRCs, COBS, debouncing, PRNGs.
- **[flexmath](math/README.md)** (`fm_`): fixed point (Q15 / Q31 / Q16.16),
  float functions without an FPU, vectors, matrices, quaternions and PID.

Plain C11, no dependencies. Each function states an error bound and a cycle
count measured on an STM32F103 (Cortex-M3, 72 MHz) next to newlib libm and
CMSIS-DSP. The bench firmware runs on
[flex](https://github.com/antonykasera/STM32F1-HAL).

## Using it

With [flex](https://github.com/antonykasera/STM32F1-HAL) v1.1.0 or later, add
flexlib as a submodule in `lib/flexlib` and name the libraries in your
Makefile, next to the flex modules:

```make
FLEX_MODULES := gpio clock usb
FLEXLIB_LIBS := math              # utils, math; math brings utils with it
```

flex's template includes the matching fragments and builds their sources with
yours. Without flex, include the fragments yourself; they only set variables:

```make
include lib/flexlib/flexlib.mk    # FLEXLIB_LIBS ?= utils math
CFLAGS += $(FLEXLIB_CFLAGS)       # include paths only: your MCU flags apply
SRCS   += $(FLEXLIB_SRCS)
```

Build with `-ffunction-sections -fdata-sections` and link with
`--gc-sections`. Define `FX_ASSERT_FAIL(file, line)` for your own assert
handler, or `FX_DISABLE_ASSERT` to compile the checks out. Each library's
README lists its other options (`FU_CRC_SMALL`, `FM_LUT_IN_RAM`).

## Libraries and prefixes

Each library has a folder, a fragment, a prefix and its own version tags
(`flexmath-v0.2.0`). Dependencies run one way: arch, utils, math, then the rest.

| library | folder | fragment | prefix | status |
|---|---|---|---|---|
| arch layer | `arch/` | (with every library) | `fx_` | released |
| flexutils | `utils/` | `flexutils.mk` | `fu_` | 0.1.0 |
| flexmath | `math/` | `flexmath.mk` | `fm_` | 0.2.0 |
| flexdsp: FIR, biquad, FFT, windows | `dsp/` | `flexdsp.mk` | `fd_` | planned |
| flexctrl: observers, Kalman, motor control | `ctrl/` | `flexctrl.mk` | `fct_` | planned |
| flexcomm: protocols, packet parsers | `comm/` | `flexcomm.mk` | `fco_` | planned |
| flexnn: int8 inference kernels | `nn/` | `flexnn.mk` | `fnn_` | planned |
| flextrace: diagnostics, logging | `trace/` | `flextrace.mk` | `ftr_` | planned |

## Status

| Part | Host | Board |
|---|---|---|
| `arch/fx_arch.h` | tests, asm-audit | golden check |
| bench firmware | builds | runs |
| `fu_bits` | exhaustive + random tests, asm-audit | bench + golden check |
| `fu_fmt` integers | all 2^32 inputs (u32, hex), tests, asm-audit | bench + golden check |
| `fu_fmt` Q16.16 and float, `fu_snprintf` | all 2^32 inputs against `%.*f`, tests, asm-audit | bench + golden check |
| `fu_crc` (`FU_CRC_SMALL` 16-entry tables) | check values, random data, asm-audit | bench + golden check |
| `fu_ring` | random operations, DMA simulation, asm-audit | bench + golden check |
| `fu_parse` | all 2^32 inputs, Q16.16 rounding ties, asm-audit | bench + golden check |
| `fu_cobs` | vectors, random frames, malformed input, asm-audit | bench + golden check |
| `fu_debounce` | reference counters, glitch properties, asm-audit | bench + golden check |
| `fu_rand` | known answers, jump, period, bias tests, asm-audit | bench + golden check |
| `fm_fixed.h` | add/sub/mul q15 on all 2^32 pairs, conversions on all 2^32 floats and values, vs CMSIS-DSP, asm-audit | bench + golden check |
| `fm_sqrt` | isqrt32 on all 2^32 inputs, Q functions on every positive input, vs CMSIS-DSP, asm-audit | bench + golden check |
| `fm_div` | q15 and 1/x on all 2^32, q31/Q16.16 on 2^32 hashed pairs, vs CMSIS-DSP, asm-audit | bench + golden check |
| `fm_trig` | q15 on all 65536 angles, q31 on all 2^32, vs CMSIS-DSP, asm-audit | bench (warm and cold) + golden check |
| `fm_atan` | atan2 q15 on all 2^32 pairs, asin/acos on every input, atan2 q31 on 2^32 hashed pairs, vs CMSIS-DSP, asm-audit | bench (warm and cold) + golden check |
| `fm_exp` | logs and exp2/exp on every input, pow on 2^32 hashed pairs, vs CMSIS-DSP, asm-audit | bench (warm and cold) + golden check |
| `fm_float`: `fm_sqrtf`, `fm_recipf`, `fm_rsqrtf` correctly rounded; `fm_sinf`, `fm_cosf`, `fm_sincosf` within 0.515 ULP; `fm_atan2f`, `fm_asinf`, `fm_acosf` within 0.545 ULP; `fm_log2f`, `fm_logf`, `fm_exp2f`, `fm_expf` within 0.52 ULP | all 2^32 floats against correctly rounded references or double; atan2 on 2^32 hashed pairs; vs CMSIS-DSP, asm-audit | bench (warm and cold) + golden check |
| `fm_linalg` | q31 against exact 128-bit sums, float against long double sums, vs CMSIS-DSP, asm-audit | bench (warm and cold) + golden check |
| `fm_quat` | long double references, drift over 10^5 steps, inf / NaN, vs CMSIS-DSP, asm-audit | bench (warm and cold) + golden check |
| `fm_pid` | Q16.16 against 128-bit arithmetic, float against long double sums, inf / NaN, asm-audit | bench + golden check |

## Layout

```
flexlib.mk, flexutils.mk, flexmath.mk    consumer fragments (variables only)
arch/            fx_arch.h: single-instruction wrappers, plain C on the host
utils/           flexutils: README, CHANGELOG, include/ (public), src/
math/            flexmath: README, CHANGELOG, include/ (public), src/
test/host/       host tests: harness, error statistics, ULP error
test/exhaustive/ every 32-bit input (make exhaustive)
test/accuracy/   error statistics against CMSIS-DSP (make accuracy)
test/asm/        asm-audit spec and probes
bench/           the F103 bench firmware: core/, cases/, port/ (see its README)
tools/           asm_audit.py, bench_run.py, bench_windows.py; gen/ table generators
docs/            results/ (bench results), accuracy.md
mk/              the Makefile's rules, one file per area
third_party/     flex and CMSIS-DSP (submodules, for the bench only)
```

## Building and testing

| Command | What it does |
|---|---|
| `make` | `host-test`, `cross`, `lint`, `asm-audit` (`make help` lists every target) |
| `make host-test [HOST_CC=clang]` | host tests with ASan + UBSan |
| `make exhaustive` | checks every 32-bit input or state where that is possible (~8 min on 12 cores) |
| `make exhaustive-slow` | the same for q16 and float text, against the C library's `%.*f` (~13 min on 12 cores) |
| `make cross` | `-Werror` builds for Cortex-M3, M4, M7 |
| `make lint` | `-Wpedantic` everywhere, each header compiled on its own |
| `make asm-audit` | checks the M3 disassembly for the expected instructions and for no soft-float or libgcc calls |
| `make tables` | regenerates the CRC and math tables (`utils/src/fu_crc_tab.c`, `math/src/fm_tables.c`) |
| `make accuracy` | flexmath and the CMSIS-DSP functions it replaces against a `double` reference; writes `docs/accuracy.md` (~1 min) |
| `make golden` | runs the bench cases on the host and writes `bench/core/golden.c` |
| `make bench [BENCH_OPT=-Os]` | builds the bench firmware: flexutils, flexmath's functions, and its structures (`BENCH_SET=utils\|math\|struct` for one) |
| `make bench-run` | flashes each image over ST-Link in turn, reads `/dev/ttyACM0`, writes `docs/results/f103.md` |
| `make bench-windows` | lists the instructions each bench case actually times |

The bench needs the submodules: `git submodule update --init`.

## Bench

[bench/](bench/README.md) times every function on the board in DWT cycles,
warm and cold, and checks each case's output against the host.

## Results

[docs/results/f103.md](docs/results/f103.md): every bench case with its
baselines. [docs/accuracy.md](docs/accuracy.md): flexmath's errors next to
CMSIS-DSP's.

## License

MIT, see [LICENSE](LICENSE). CMSIS-DSP (bench baseline only) is Apache-2.0.
