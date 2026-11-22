# Testing

`make` runs the host tests, the Cortex-M3 / M4 / M7 builds, the lint pass and
the disassembly checks. CI runs them with GCC 13, and the host tests again
with clang.

Make uses every core unless you pass `-j` yourself, and keeps each build's
objects: after a change, only what depends on it is compiled again. The host
tests print one summary line per runner. A runner's full output, error
statistics included, is in its log under `build/host-<compiler>/`, and a
failing runner prints it.

## Commands

| command | what it does |
| --- | --- |
| `make` | host tests, cross builds, lint, asm-audit (`make help` lists every target) |
| `make host-test [HOST_CC=clang]` | host tests with ASan and UBSan |
| `make exhaustive` | every 32-bit input where that is possible (~14 min) |
| `make exhaustive-slow` | the same for text formatting and the float block conversions (~15 min) |
| `make accuracy` | errors of flexlib and CMSIS-DSP, written to `docs/accuracy.md` (~3 min) |
| `make golden` | the bench cases run on the host, written to `bench/core/golden*.c` |
| `make bench` | the bench firmware (`BENCH_SET=` one image, `BENCH_TARGET=f405`) |
| `make bench-run` | flashes and runs each image, writes `docs/results/f103.md` |
| `make deps` | fetches the submodules the bench needs |
| `make dist`, `make consumer` | the release archive, and a blank project built against it |
| `make cmake-check` | a CMake project builds against the archive, on the host and for the Cortex-M3 |
| `make nn-check` | flexnn against the TFLite interpreter's reference kernels, on models written without TensorFlow (needs uv) |

## Terms

- **exhaustive**: every possible input is checked, for example all 2^32
  values of a 32-bit argument.
- **asm-audit**: the compiled code is checked for the instructions it should
  use, and for calls it should not make (soft float, libgcc division).
- **bench**: the function runs on the board and its time is measured in CPU
  cycles. Warm means the code is already in the flash cache. Cold means the
  cache is flushed first.
- **golden check**: the board's output for each bench case is hashed and
  compared with the hash the host computes.

## What the host tests compare against

Every module also gets asm-audit, a bench run and a golden check.

| modules | reference |
| --- | --- |
| `fu_fmt`, `fu_parse` | every 32-bit input; float and Q16.16 text against the C library's `%.*f` |
| other `fu_` modules | known answers, random data, simple reference implementations |
| `fm_fixed`, `fm_sqrt`, `fm_div`, `fm_trig`, `fm_atan`, `fm_exp`, `fm_float` | every input where there are at most 2^32, otherwise 2^32 hashed pairs; correctly rounded or `double` results |
| `fm_linalg`, `fm_quat`, `fm_pid` | exact 128-bit or `long double` sums |
| `fd_` fixed point | bit-exact against 128-bit integer models of the same arithmetic |
| `fd_` float | `__float128` references |
| `fd_window`, `fd_tone`, `fd_noise` | `long double`; noise statistics |
| `fnn_` | bit-exact against 64-bit models written in TFLite's, gemmlowp's and CMSIS-NN's terms, in each rounding mode; every int32 sum for requantisation, and every float for quantising |

[accuracy.md](accuracy.md) has the errors next to CMSIS-DSP's.
