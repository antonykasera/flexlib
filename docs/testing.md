# Testing

`make` runs the host tests, the Cortex-M3 / M4 / M7 builds, the lint pass and
the disassembly checks. CI runs them with GCC 13, and the host tests again
with clang.

Make runs on all cores unless `-j` is given, and keeps the objects of previous
builds. After a change, the files that depend on it are recompiled. The host
tests print one summary line per runner. A runner's full output, error
statistics included, is in its log under `build/host-<compiler>/`, and a
failing runner prints it.

## Commands

| command | what it does |
| --- | --- |
| `make` | host tests, cross builds, lint, asm-audit (`make help` lists the targets) |
| `make host-test [HOST_CC=clang]` | host tests with ASan and UBSan |
| `make exhaustive` | all 2^32 inputs of the 32-bit functions where feasible (~14 min) |
| `make exhaustive-slow` | the same for text formatting and the float block conversions (~15 min) |
| `make accuracy` | errors of flexlib and CMSIS-DSP, written to `docs/accuracy.md` (~3 min) |
| `make golden` | the bench cases run on the host, written to `bench/core/golden*.c` |
| `make bench` | the bench firmware (`BENCH_SET=` one image, `BENCH_TARGET=f405`) |
| `make bench-run` | flashes and runs the bench images, writes `docs/results/f103.md` |
| `make deps` | fetches the submodules the bench needs |
| `make dist`, `make consumer` | the release archive, and a blank project built against it |
| `make cmake-check` | a CMake project builds against the archive, on the host and for the Cortex-M3 |
| `make nn-check` | flexnn and its converter against the TFLite interpreter's reference kernels, on models written without TensorFlow and on the ones in `test/nn/models/` (needs uv) |
| `python3 test/nn/gen_models.py` | rewrites `test/nn/models/` with TensorFlow's own converter (needs TensorFlow) |
| `make nn-cmsis` | flexnn in `FNN_ROUND_GEMMLOWP` against CMSIS-NN built for the host, layer by layer, for identical outputs (needs `make deps`) |

## Terms

- **exhaustive**: all possible inputs are checked, for example the 2^32
  values of a 32-bit argument.
- **asm-audit**: the compiled code is checked for the instructions it should
  use, and for calls it should not make (soft float, libgcc division).
- **bench**: the function runs on the board and its time is measured in CPU
  cycles. Warm means the code is already in the flash cache. Cold means the
  cache is flushed first.
- **golden check**: the board's outputs for the bench cases are hashed and
  compared with the hashes the host computes.

## What the host tests compare against

All modules are also covered by asm-audit, a bench run and a golden check.

| modules | reference |
| --- | --- |
| `fu_fmt`, `fu_parse` | all 2^32 inputs, and float and Q16.16 text against the C library's `%.*f` |
| other `fu_` modules | known answers, random data, simple reference implementations |
| `fm_fixed`, `fm_sqrt`, `fm_div`, `fm_trig`, `fm_atan`, `fm_exp`, `fm_float` | all inputs where there are at most 2^32, otherwise 2^32 hashed pairs, against correctly rounded or `double` results |
| `fm_linalg`, `fm_quat`, `fm_pid` | exact 128-bit or `long double` sums |
| `fd_` fixed point | identical results to 128-bit integer models of the same arithmetic |
| `fd_` float | `__float128` references |
| `fd_window`, `fd_tone`, `fd_noise` | `long double`, and noise statistics |
| `fnn_` | identical results to 64-bit models written in TFLite's, gemmlowp's and CMSIS-NN's terms, in all rounding modes, with all int32 sums for requantisation and all floats for quantising |

[accuracy.md](accuracy.md) has the errors next to CMSIS-DSP's.
