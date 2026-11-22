"""Runs small models in the TFLite interpreter and the same arithmetic through
flexnn. FNN_ROUND_AWAY has to match on every output."""

import ctypes
import math
import sys
from pathlib import Path

import numpy as np

import tfl

MODES = ("FNN_ROUND_UP", "FNN_ROUND_AWAY", "FNN_ROUND_GEMMLOWP")
AWAY = 1
RELU, RELU6 = 1, 3
UNARY = ctypes.CFUNCTYPE(ctypes.c_double, ctypes.c_double)


def load(libdir):
    libs = []
    for m in range(len(MODES)):
        lib = ctypes.CDLL(str(Path(libdir) / f"libfnn{m}.so"))
        lib.fnn_quant_from_scale.argtypes = [
            ctypes.c_double, ctypes.POINTER(ctypes.c_int32),
            ctypes.POINTER(ctypes.c_int8)]
        lib.shim_requant_s8.argtypes = [ctypes.c_int32] * 6
        lib.shim_requant_s8.restype = ctypes.c_int8
        lib.shim_dense_s8.argtypes = [
            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32,
            ctypes.c_uint32, ctypes.c_void_p, ctypes.c_int32, ctypes.c_double,
            ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_uint32,
            ctypes.c_void_p, ctypes.c_void_p]
        lib.fnn_lut_s8_build.argtypes = [
            UNARY, ctypes.c_double, ctypes.c_int32, ctypes.c_double,
            ctypes.c_int32, ctypes.c_void_p]
        lib.fnn_lut_s8.argtypes = [ctypes.c_void_p] * 2 + [
            ctypes.c_uint32, ctypes.c_void_p]
        assert lib.shim_rounding() == m, f"libfnn{m}.so is mode {lib.shim_rounding()}"
        libs.append(lib)
    return libs


def tfl_round(v):
    """Rounds half away from zero as TfLiteRound does."""
    return int(np.sign(v) * np.floor(abs(v) + 0.5))


def act_range(act, s_out, zy):
    """The int8 clamp TFLite's CalculateActivationRangeQuantized gives."""
    f32 = np.float32
    lo, hi = -128, 127
    if act in (RELU, RELU6):
        lo = max(lo, zy)
    if act == RELU6:
        hi = min(hi, zy + tfl_round(float(f32(6.0) / f32(s_out))))
    return lo, hi


def case_fc_requant(libs, s_in, s_w, s_out, zx, zy, step, act=0, n=16):
    """A fully connected layer with one input and weights of 1. Each output is
    then x - zx + bias requantised, for every int8 x and n biases."""
    bias = np.array([step * (j - n // 2) for j in range(n)], np.int32)
    m = tfl.Model()
    x = m.tensor("x", [1, 1], tfl.INT8, scale=s_in, zp=zx)
    w = m.tensor("w", [n, 1], tfl.INT8, scale=s_w, zp=0, data=np.ones((n, 1)))
    b = m.tensor("b", [n], tfl.INT32, scale=float(np.float32(s_in) * np.float32(s_w)),
                 zp=0, data=bias)
    y = m.tensor("y", [1, n], tfl.INT8, scale=s_out, zp=zy)
    m.op("FULLY_CONNECTED", [x, w, b], [y], FusedActivationFunction=act)
    xs = list(range(-128, 128))
    got = tfl.run_reference(m.build(), [np.array([[v]], np.int8) for v in xs])

    eff = float(np.float32(s_in) * np.float32(s_w)) / float(np.float32(s_out))
    lo, hi = act_range(act, s_out, zy)
    bad = []
    for lib in libs:
        mult, rsh = ctypes.c_int32(), ctypes.c_int8()
        assert lib.fnn_quant_from_scale(eff, ctypes.byref(mult), ctypes.byref(rsh)) == 0
        wrong = 0
        for xv, out in zip(xs, got):
            for j in range(n):
                acc = xv - zx + int(bias[j])
                v = lib.shim_requant_s8(acc, mult.value, rsh.value, zy, lo, hi)
                wrong += v != int(out[0, j])
        bad.append(wrong)
    name = (f"fully connected, scale {eff:.6g}, zero points {zx} / {zy}"
            + {0: "", RELU: ", ReLU", RELU6: ", ReLU6"}[act])
    return name, 256 * n, bad


def case_dense(libs, n_in, n_out, act, seed, runs=200):
    """A fully connected layer with random weights, biases and zero points,
    in both weight layouts."""
    rng = np.random.default_rng(seed)
    s_in, s_w, s_out = 0.05, 0.004, 0.25
    zx, zy = int(rng.integers(-128, 128)), int(rng.integers(-128, 128))
    w = rng.integers(-127, 128, (n_out, n_in), dtype=np.int8)
    bias = rng.integers(-20000, 20000, n_out, dtype=np.int32)
    m = tfl.Model()
    x = m.tensor("x", [1, n_in], tfl.INT8, scale=s_in, zp=zx)
    wt = m.tensor("w", [n_out, n_in], tfl.INT8, scale=s_w, zp=0, data=w)
    b = m.tensor("b", [n_out], tfl.INT32,
                 scale=float(np.float32(s_in) * np.float32(s_w)), zp=0, data=bias)
    y = m.tensor("y", [1, n_out], tfl.INT8, scale=s_out, zp=zy)
    m.op("FULLY_CONNECTED", [x, wt, b], [y], FusedActivationFunction=act)
    xs = [rng.integers(-128, 128, (1, n_in), dtype=np.int8) for _ in range(runs)]
    got = tfl.run_reference(m.build(), xs)

    eff = float(np.float32(s_in) * np.float32(s_w)) / float(np.float32(s_out))
    lo, hi = act_range(act, s_out, zy)
    wc = np.ascontiguousarray(w)
    packed = np.zeros(n_in * n_out, np.int8)
    bf = np.zeros(n_out, np.int32)
    out = np.zeros(n_out, np.int8)
    bad = []
    for lib in libs:
        wrong = 0
        for use_packed in (0, 1):
            for xv, ref in zip(xs, got):
                xc = np.ascontiguousarray(xv[0])
                lib.shim_dense_s8(xc.ctypes.data, wc.ctypes.data, packed.ctypes.data,
                                  n_in, n_out, bias.ctypes.data, zx, eff, zy, lo, hi,
                                  use_packed, bf.ctypes.data, out.ctypes.data)
                wrong += int(np.count_nonzero(out != ref[0]))
        bad.append(wrong)
    name = (f"fully connected {n_in} -> {n_out}, zero points {zx} / {zy}"
            + {0: "", RELU: ", ReLU", RELU6: ", ReLU6"}[act])
    return name, 2 * runs * n_out, bad


def case_table(lib, op, s_in, zx, s_out, zy, f, seed, **options):
    """A one-input operator, as an exact table and as one read back from TFLite."""
    m = tfl.Model()
    x = m.tensor("x", [1, 256], tfl.INT8, scale=s_in, zp=zx)
    y = m.tensor("y", [1, 256], tfl.INT8, scale=s_out, zp=zy)
    m.op(op, [x], [y], **options)
    blob = m.build()
    every = np.arange(-128, 128, dtype=np.int8).reshape(1, 256)
    ref = tfl.run_reference(blob, [every])[0][0]

    exact = np.zeros(256, np.int8)
    assert lib.fnn_lut_s8_build(UNARY(f), s_in, zx, s_out, zy,
                                exact.ctypes.data) == 0
    exact = exact[every[0].view(np.uint8)]
    diff = np.abs(exact.astype(int) - ref.astype(int))

    read = np.zeros(256, np.int8)
    read[every[0].view(np.uint8)] = ref
    rng = np.random.default_rng(seed)
    xs = [rng.integers(-128, 128, (1, 256), dtype=np.int8) for _ in range(20)]
    got = tfl.run_reference(blob, xs)
    out = np.zeros(256, np.int8)
    wrong = 0
    for xv, want in zip(xs, got):
        xc = np.ascontiguousarray(xv[0])
        lib.fnn_lut_s8(xc.ctypes.data, out.ctypes.data, 256, read.ctypes.data)
        wrong += int(np.count_nonzero(out != want[0]))
    name = f"{op.lower()}, scales {s_in:.4g} / {s_out:.4g}, zero points {zx} / {zy}"
    return name, int(np.count_nonzero(diff)), int(diff.max()), 20 * 256, wrong


def logistic(v):
    return 1.0 / (1.0 + math.exp(-v))


def hard_swish(v):
    return v * min(max(v + 3.0, 0.0), 6.0) / 6.0


def main():
    libs = load(sys.argv[1] if len(sys.argv) > 1 else "build/nn-check")
    cases = [
        (1.0, 1.0, 256.0, 0, 0, 128),           # a scale of 2^-8 makes exact ties
        (1.0, 1.0, 256.0 / 3.0, -3, 5, 37),
        (0.02, 0.5, 0.125, 7, -10, 1000),
        (0.5, 0.5, 0.2, 0, 0, 3),               # 1.25 takes the slower path
        (1e-3, 1e-3, 1e-5, 0, 0, 1),
        (0.1, 0.03, 0.05, -128, -128, 50, RELU),
        (0.1, 0.03, 0.05, 2, 3, 50, RELU6),
        (0.0625, 0.0625, 1.0 / 512.0, -1, 1, 64),  # so does 2, exactly
    ]
    failed = False
    print(f"{'case':58} {'outputs':>8}  " + "  ".join(f"{m[10:]:>8}" for m in MODES))
    for c in cases:
        name, n, bad = case_fc_requant(libs, *c)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[AWAY] != 0
    for c in ((256, 64, 0, 1), (64, 12, RELU, 2), (17, 13, RELU6, 3), (1, 70, 0, 4)):
        name, n, bad = case_dense(libs, *c)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[AWAY] != 0
    print("\nmismatches per rounding mode; FNN_ROUND_AWAY must have none\n")

    tables = [
        ("LOGISTIC", 1.0 / 16.0, 0, 1.0 / 256.0, -128, logistic, 5),
        ("LOGISTIC", 0.1, -20, 1.0 / 256.0, -128, logistic, 6),
        ("TANH", 1.0 / 32.0, 0, 1.0 / 128.0, 0, math.tanh, 7),
        ("TANH", 0.05, 10, 1.0 / 128.0, 0, math.tanh, 8),
        ("HARD_SWISH", 0.05, -10, 0.04, -100, hard_swish, 9),
        ("LEAKY_RELU", 0.05, 3, 0.04, -20, lambda v: v if v >= 0 else 0.2 * v, 10),
    ]
    print(f"{'table':58} {'differ':>6} {'by':>3}  {'outputs':>8} {'wrong':>6}")
    for op, *c in tables:
        opts = {"Alpha": 0.2} if op == "LEAKY_RELU" else {}
        name, differ, by, n, wrong = case_table(libs[AWAY], op, *c, **opts)
        print(f"{name:58} {differ:6} {by:3}  {n:8} {wrong:6}")
        failed |= wrong != 0
    print("\nentries of the exact table that differ from TFLite's, and by how much;"
          "\nthe table read from the interpreter must give no wrong outputs")
    if failed:
        sys.exit("nn-check: flexnn differs from the TFLite reference kernels")
    print("nn-check: flexnn matches the TFLite reference kernels")


if __name__ == "__main__":
    main()
