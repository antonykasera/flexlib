"""Writes R2, R3 and R5, converted, to bench/models/ for the bench.

    build/nn-venv/bin/python test/nn/bench_models.py bench/models

Copyright (c) 2026 Antony Kasera
SPDX-License-Identifier: Apache-2.0
"""

import sys
import tempfile
from pathlib import Path

import numpy as np

import check
import fnn_tflite

MODELS = (("r2", check.net_r2, 2), ("r3", check.net_r3, 3), ("r5", check.net_r5, 5))


def write(out_dir):
    out = Path(out_dir)
    with tempfile.TemporaryDirectory() as tmp:
        for name, make, seed in MODELS:
            path = Path(tmp) / f"{name}.tflite"
            path.write_bytes(make(np.random.default_rng(seed)))
            fnn_tflite.convert(path, out, name)


def stale(out_dir):
    with tempfile.TemporaryDirectory() as tmp:
        write(tmp)
        return [p.name for p in sorted(Path(tmp).glob("*.[ch]"))
                if not (Path(out_dir) / p.name).exists() or
                (Path(out_dir) / p.name).read_text() != p.read_text()]


if __name__ == "__main__":
    write(sys.argv[1] if len(sys.argv) > 1 else "bench/models")
