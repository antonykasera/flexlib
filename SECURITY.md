# Security policy

## Supported versions

flexlib is before 1.0. Fixes go into the next release. The latest
release is the only one supported.

## What counts as a security issue

Mainly memory safety: a function that writes or reads outside the buffers
it was given, or hits undefined behaviour, on arguments its header allows.
The parsers, COBS decoding, the rings and the formatting functions are the
likely places, because they handle data from outside the device.

A result outside its stated error bound, or a wrong result, is a bug.
Report it as a normal issue.

## Reporting

Please do not open a public issue for a security problem. Report it
privately instead:

- On GitHub: [report a vulnerability](https://github.com/antonykasera/flexlib/security/advisories/new)
  from the repository's Security tab. Only you and the maintainer can see
  the report.
- By email, if you cannot use GitHub: <antonykasera.dev@gmail.com>

Include the flexlib version, the function, the arguments that trigger the
problem, and the target and compiler if they matter. A small program that
shows it helps most.

You will get a reply once the report has been read, and the fix will be
discussed with you before anything is published. When it is released, the
advisory credits you unless you ask otherwise.
