# Changelog

Repository releases. Per library: [utils/CHANGELOG.md](utils/CHANGELOG.md),
[math/CHANGELOG.md](math/CHANGELOG.md).

## Unreleased

- Each library has its own `include/` and `src/`; use `$(FLEXLIB_CFLAGS)` for include paths.
- `flexlib.mk`: all libraries named in `FLEXLIB_LIBS` in one include.
- The fragments can be included in any order and more than once.
- Bench moved to `bench/core`, `bench/cases`, `bench/port/f103`; results in `docs/results/`; flex and CMSIS-DSP under `third_party/`.

## v0.1.0 (2026-11-09)

First release: flexutils 0.1.0 and flexmath 0.2.0, with F103 bench results and the CMSIS-DSP accuracy comparison.
