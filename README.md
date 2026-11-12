# flexlib

[![CI](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/antonykasera/flexlib/actions/workflows/ci.yml?query=branch%3Amaster)
[![Release](https://img.shields.io/github/v/release/antonykasera/flexlib)](https://github.com/antonykasera/flexlib/releases/latest)

Compute libraries for Cortex-M, measured on the chip they're for:

- **[flexutils](utils/README.md)** (`fu_`): bit tricks, rings, formatting and
  parsing, CRCs, COBS, debouncing, PRNGs.
- **[flexmath](math/README.md)** (`fm_`): fixed point (Q15 / Q31 / Q16.16),
  float functions without an FPU, vectors, matrices, quaternions and PID.
- **[flexdsp](dsp/README.md)** (`fd_`): block arithmetic, FIR and biquad
  filters, windows, tone detection and generation, noise, FFTs, convolution
  and correlation, adaptive filters.

Plain C11, no dependencies. Each function states an error bound and a cycle
count measured on an STM32F103 (Cortex-M3, 72 MHz) next to newlib libm and
CMSIS-DSP. The bench firmware runs on
[flex](https://github.com/antonykasera/STM32F1-HAL).

## Using it

Put flexlib in `lib/flexlib`: as a submodule pinned to a release, or from the
release archive (the libraries only, about 100 KB):

```sh
git submodule add https://github.com/antonykasera/flexlib.git lib/flexlib
git -C lib/flexlib checkout v0.4.0
# or
tar xf flexlib-0.4.0.tar.gz -C lib
```

With [flex](https://github.com/antonykasera/STM32F1-HAL) v1.1.2 or later,
start from flex's template and name the libraries next to the flex modules:

```make
FLEX_MODULES := gpio clock usb
FLEXLIB_LIBS := math              # utils, math, dsp; each brings the ones it uses
```

Without flex, include the fragments yourself; they only set variables:

```make
include lib/flexlib/flexlib.mk    # FLEXLIB_LIBS ?= utils math dsp
CFLAGS += $(FLEXLIB_CFLAGS)       # include paths only: your MCU flags apply
SRCS   += $(FLEXLIB_SRCS)
```

Build with `-ffunction-sections -fdata-sections` and link with
`--gc-sections`. Define `FX_ASSERT_FAIL(file, line)` for your own assert
handler, or `FX_DISABLE_ASSERT` to compile the checks out. Each library's
README lists its other options (`FU_CRC_SMALL`, `FM_LUT_IN_RAM`). Set them in
your own Makefile, not in `lib/flexlib`, so upgrades don't alter them.

`FLEXLIB_REQUIRE := 0.4.0` before the include stops the build if `lib/flexlib`
is older; in C, `fx_version.h` has `FLEXLIB_VERSION` and `FLEXLIB_VERSION_NUM`.

### Upgrading

Check the release notes and each library's CHANGELOG first: before 1.0, a
minor release may change an API (with upgrade notes); a patch release never
does. Then check out the new tag (`git -C lib/flexlib checkout v0.4.0`) or
replace `lib/flexlib` with the new archive, and rebuild. flexlib's own
submodules are for its bench and are skipped by `git submodule update
--recursive`.

## Libraries and prefixes

Each library has a folder, a fragment, a prefix and its own version tags
(`flexdsp-v0.2.0`). Dependencies run one way: arch, utils, math, then the rest.

| library | folder | fragment | prefix | status |
| --- | --- | --- | --- | --- |
| arch layer | `arch/` | (with every library) | `fx_` | released |
| flexutils | `utils/` | `flexutils.mk` | `fu_` | 0.1.1 |
| flexmath | `math/` | `flexmath.mk` | `fm_` | 0.2.3 |
| flexdsp | `dsp/` | `flexdsp.mk` | `fd_` | 0.2.0 |
| flexctrl: observers, Kalman, motor control | `ctrl/` | `flexctrl.mk` | `fct_` | planned |
| flexcomm: protocols, packet parsers | `comm/` | `flexcomm.mk` | `fco_` | planned |
| flexnn: int8 inference kernels | `nn/` | `flexnn.mk` | `fnn_` | planned |
| flextrace: diagnostics, logging | `trace/` | `flextrace.mk` | `ftr_` | planned |

## Status

| Part | Host | Board |
| --- | --- | --- |
| `arch/fx_arch.h` | tests, asm-audit | golden check |
| `fx_arch.h` packed pairs (`fx_smlad`, `fx_qadd16`, ...) | edge and random pairs against arithmetic on the halves, M4 asm-audit | bench (M3 C path) + golden check |
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
| `fd_corr`, `fd_lms` | convolution and correlation bit-exact against 128-bit sums at every size to 16 x 16 (10^6 blocks) and larger, every lag range, the accumulator bound and its integer corner cases; autocorrelation exponents and rounding carry; the peak vertex for all 2^32 neighbour pairs; Levinson bit-exact against a 128-bit recursion, and against double; correlation through FFTs on delays, noise and chirps; LMS bit-exact against the textbook recursion in all three modes, every split and chunk, convergence; vs CMSIS-DSP, asm-audit | bench + golden check |
| `fd_fft` | every size up to 4096, complex, real and inverse, bit-exact against an integer model of every stage that also checks no stage leaves its type; corner inputs that reach each stage's growth bound; every bin of every size up to 1024 against the exact DFT of the quantised tone; scaling invariance, round trips; vs CMSIS-DSP, asm-audit | bench + golden check |
| `fd_window`, `fd_tone`, `fd_noise` | every window type at every length up to 4096 (both symmetries) against long double; Goertzel bit-exact against a 128-bit recursion and against the DFT; NCO against fm_sin / cos; oscillator drift over 10^6 samples; noise mean, variance, kurtosis, tails, histogram; asm-audit | bench + golden check |
| `fd_biquad`, `fd_filt` | bit-exact against 128-bit recursions (q15, q31, error feedback, wide sums, split calls, in place), saturation and tie boundaries, design against double RBJ and pole precision at fc 1e-4, Butterworth -3 dB, noise shaping +51 dB; vs CMSIS-DSP, asm-audit | bench + golden check |
| `fd_fir` | every tap count 1-9 and unroll edges, chunk sizes, split calls, in place, gain-2 boundaries, decimation phases and interpolator branches against exact 128-bit convolutions; design against a double reference and window sidelobes; vs CMSIS-DSP, asm-audit | bench + golden check |
| `fd_vec` | q15 add / sub / mul, complex magnitude and multiply, q31 to q15 on all 2^32 inputs; statistics against exact 128-bit sums; loop tails, in place, overruns; vs CMSIS-DSP, asm-audit | bench + golden check |

## Layout

```
flexlib.mk, flexutils.mk, flexmath.mk, flexdsp.mk
                 consumer fragments (variables only)
arch/            fx_arch.h: single-instruction wrappers, plain C on the host;
                 fx_version.h
utils/           flexutils: README, CHANGELOG, include/ (public), src/
math/            flexmath: README, CHANGELOG, include/ (public), src/
dsp/             flexdsp: README, CHANGELOG, include/ (public), src/
test/host/       host tests: harness, error and SNR statistics, ULP error, DSP references
test/exhaustive/ every 32-bit input (make exhaustive)
test/accuracy/   error statistics against CMSIS-DSP (make accuracy)
test/asm/        asm-audit spec and probes
bench/           the F103 bench firmware: core/, cases/, port/ (see its README)
examples/        flexmeter: a USB bench instrument on flex
tools/           asm_audit.py, bench_run.py, bench_windows.py; gen/ table generators
docs/            results/ (bench results), accuracy.md
mk/              the Makefile's rules, one file per area
third_party/     flex and CMSIS-DSP (submodules, for the bench only)
```

## Building and testing

| Command | What it does |
| --- | --- |
| `make` | `host-test`, `cross`, `lint`, `asm-audit` (`make help` lists every target) |
| `make host-test [HOST_CC=clang]` | host tests with ASan + UBSan |
| `make exhaustive` | checks every 32-bit input or state where that is possible (~8 min on 12 cores) |
| `make exhaustive-slow` | the same for q16 and float text, against the C library's `%.*f` (~13 min on 12 cores) |
| `make cross` | `-Werror` builds for Cortex-M3, M4, M7 |
| `make lint` | `-Wpedantic` everywhere, each header compiled on its own |
| `make asm-audit` | checks the M3 disassembly for the expected instructions and for no soft-float or libgcc calls, and the M4 disassembly of the DSP-extension paths |
| `make tables` | regenerates the CRC, math and FFT tables (`utils/src/fu_crc_tab.c`, `math/src/fm_tables.c`, `dsp/src/fd_fft_tab.h`) |
| `make accuracy` | flexmath, flexdsp and the CMSIS-DSP functions they replace against `double` or exact references; writes `docs/accuracy.md` (~3 min) |
| `make golden` | runs the bench cases on the host and writes `bench/core/golden.c` |
| `make bench [BENCH_OPT=-Os]` | builds the bench firmware: flexutils, flexmath's functions, its structures, and flexdsp in four (`BENCH_SET=utils\|math\|struct\|dsp\|dsp2\|dsp3\|dsp4` for one) |
| `make bench-run` | flashes each image over ST-Link in turn, reads `/dev/ttyACM0`, writes `docs/results/f103.md` |
| `make bench-windows` | lists the instructions each bench case actually times |
| `make deps` | fetches flex and CMSIS-DSP for the bench, accuracy and examples (`make check` needs neither) |
| `make dist` | the release archive and its SHA-256: `build/dist/flexlib-<version>.tar.gz` |
| `make consumer` | builds a blank project against that archive: flex's template, and host gcc |

## Bench

[bench/](https://github.com/antonykasera/flexlib/blob/master/bench/README.md) times every function on the board in DWT cycles,
warm and cold, and checks each case's output against the host.

## Results

[docs/results/f103.md](docs/results/f103.md): every bench case with its
baselines. [docs/accuracy.md](docs/accuracy.md): flexmath's and flexdsp's errors next to
CMSIS-DSP's.

## License

Apache License 2.0, see [LICENSE](LICENSE). If you ship flexlib in a product,
include the attribution in [NOTICE](NOTICE) with the product's other
third-party notices (section 4(d) of the license).

Releases up to v0.4.0 were published under the MIT License. The copyright
holder also grants every earlier release and commit under Apache-2.0.

CMSIS-DSP (bench baseline only) is Apache-2.0.
