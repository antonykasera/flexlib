# Changelog

Repository releases. Per library: [utils/CHANGELOG.md](utils/CHANGELOG.md),
[math/CHANGELOG.md](math/CHANGELOG.md).

## Unreleased

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
