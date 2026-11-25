## What this changes

<!-- What the change does and why. Link the issue if there is one. -->

## Checklist

- [ ] `make` passes, and `make host-test HOST_CC=clang`
- [ ] New or changed public functions are documented in their header, with
      the error bound (`make docs-check`)
- [ ] Tests show the bound holds (exhaustive where the input space allows)
- [ ] A new kernel has an asm-audit entry and a bench case (`make golden`)
- [ ] User-visible changes have a line under `## Unreleased` in the
      library's `CHANGELOG.md`
- [ ] Commits are limited to one concern and build individually

## Measurements

<!-- Cycle counts from the board (make bench-run) or the accuracy run, if
the change affects them. If you have no board, say so and the maintainer
will run the bench. -->
