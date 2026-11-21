# Contributing

Thanks for taking the time to help. Bug reports, accuracy and performance
findings, fixes and new functions are all welcome. This page explains what a
change needs to be merged without a long back and forth.

## Before you start

For a bug fix, open a pull request directly, ideally with a test that fails
before the fix. For anything larger, a new function, a new module or a
changed API, open an issue first. flexlib's functions come with a contract,
an error bound and a measured cycle count, and it saves you work to agree on
those before writing the code.

## Building and testing

You need `make`, a host C compiler (gcc or clang), `arm-none-eabi-gcc` with
newlib, Python 3 and Doxygen. Then, from the repository root:

```sh
make                        # host tests, Cortex-M3/M4/M7 builds, lint, asm-audit
make host-test HOST_CC=clang
make docs-check             # every public header documented
```

CI runs the same steps with GCC 13 and clang. [docs/testing.md](docs/testing.md)
lists every command, including the exhaustive runs, the accuracy tables and
the bench.

## What a change needs

- **C11, no dynamic allocation, no new dependencies.** The code builds
  without warnings under `-Wall -Wextra -Wshadow -Wconversion
  -Wsign-conversion -Wdouble-promotion`, and the lint pass adds
  `-Wpedantic`. Format it with the repository's `.clang-format`.
- **A documented contract.** Each public function has a short Doxygen
  comment in its header that states what it computes and its error bound,
  for example "within 0.5 LSB" or "correctly rounded".
- **Tests that show the bound holds.** Host tests go in `test/host/`. Where
  the input space is 2^32 or smaller, check all of it in
  `test/exhaustive/`. Otherwise use seeded random inputs plus the edge
  cases.
- **Errors in the right place.** Programmer mistakes, such as a zero length
  or a bad shift, are `FX_ASSERT`s. Failures that can happen at run time
  return a status.
- **Measured speed.** A new kernel gets an entry in `test/asm/audit.txt` and
  a bench case in `bench/cases/`, then `make golden`. Cycle counts come from
  the board (`make bench-run`). If you have no STM32F103 to run it on, say so
  in the pull request and the maintainer will run it.

## Commits and pull requests

Keep each commit to one concern, and make sure each one builds and passes on
its own. Commit subjects name the area first, as in the existing history:
`feat(dsp): ...`, `fix(math): ...`, `test(utils): ...`, `docs: ...`,
`bench: ...`.

A change that users will notice gets a line under `## Unreleased` in the
library's `CHANGELOG.md` (`utils/`, `math/` or `dsp/`). Comment and
documentation fixes do not need one.

## License

flexlib is licensed under Apache-2.0, and contributions are accepted under
Apache-2.0 only. By opening a pull request you agree that your contribution
is licensed under the Apache License 2.0, and that you have the right to
license it so. A contribution offered under any other license, or with
conditions attached, will not be merged.

Code copied from another project can only come in if its license is
compatible with Apache-2.0 and its notices come with it. Say where it came
from in the pull request.
