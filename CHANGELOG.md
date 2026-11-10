# Changelog

Repository releases. Per library: [utils/CHANGELOG.md](utils/CHANGELOG.md),
[math/CHANGELOG.md](math/CHANGELOG.md), [dsp/CHANGELOG.md](dsp/CHANGELOG.md).

## Unreleased

- flexdsp (`dsp/`, `flexdsp.mk`, prefix `fd_`): the library's folder, fragment and shared types; `FLEXLIB_LIBS` includes it by default.
- `fx_arch.h`: packed halfword-pair wrappers (`fx_smlad`, `fx_smlald`, `fx_smuad`, `fx_smusdx`, `fx_qadd16`, `fx_qsub16`, `fx_shadd16`, `fx_sxtb16`, `FX_SSAT16`, `fx_pkhbt`, `fx_pkhtb`, `fx_smmulr`, `fx_smulwb`, `fx_smlawb`): one instruction each on Cortex-M4/M7, plain C with the same results elsewhere.
- `fx_arch.h`: `FX_HAVE_FPU`, `FX_DSP_PATH` (`FX_EMULATE_DSP` runs the DSP-extension paths on the C versions), `FX_RESTRICT`, `FX_LIKELY` / `FX_UNLIKELY`, `FX_ALIGN`, `FX_UNROLL`.
- Bench: a fourth image, `BENCH_SET=dsp`.

## v0.2.0 (2026-11-10)

flexutils 0.1.1 and flexmath 0.2.1, and the first release archive.

- Each library has its own `include/` and `src/`; use `$(FLEXLIB_CFLAGS)` for include paths.
- `flexlib.mk`: all libraries named in `FLEXLIB_LIBS` in one include.
- The fragments can be included in any order and more than once.
- Release archive (`make dist`): the libraries, fragments and docs, about 70 KB.
- `fx_version.h` (`FLEXLIB_VERSION`, `FLEXLIB_VERSION_NUM`); the fragments set `FLEXLIB_VERSION`, and `FLEXLIB_REQUIRE` stops the build on an older copy.
- GCC 13: `fx_clz`, `fx_ctz` and `fx_rev16` compile to single instructions with no zero checks; the float functions are 5-23 instructions shorter.
- flex and CMSIS-DSP are fetched by `make deps`; `git submodule update --recursive` in a project skips them.
- Bench moved to `bench/core`, `bench/cases`, `bench/port/f103`; results in `docs/results/`; flex and CMSIS-DSP under `third_party/`.

## v0.1.0 (2026-11-09)

First release: flexutils 0.1.0 and flexmath 0.2.0, with F103 bench results and the CMSIS-DSP accuracy comparison.
