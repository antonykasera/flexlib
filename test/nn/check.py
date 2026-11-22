"""Runs small models in the TFLite interpreter and the same arithmetic through
flexnn, and checks that they agree in the rounding mode TFLite uses for each."""

import ctypes
import math
import sys
from pathlib import Path

import numpy as np

import tfl

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "nn" / "tools"))
import fnn_tflite  # noqa: E402

MODES = ("FNN_ROUND_UP", "FNN_ROUND_AWAY", "FNN_ROUND_GEMMLOWP")
AWAY, GEMMLOWP = 1, 2
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
        lib.fnn_softmax_s8_build.argtypes = [
            ctypes.c_double, ctypes.c_double, ctypes.c_void_p]
        lib.fnn_softmax_s8.argtypes = [ctypes.c_void_p] * 2 + [
            ctypes.c_uint32, ctypes.c_void_p]
        lib.fnn_argmax_s8.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        for f in (lib.fnn_quantize_f32_s8, lib.fnn_quantize_q15_s8):
            f.argtypes = [ctypes.c_void_p] * 2 + [ctypes.c_uint32] + [
                ctypes.c_int32] * 3
        lib.fnn_dequantize_s8_f32.argtypes = [ctypes.c_void_p] * 2 + [
            ctypes.c_uint32, ctypes.c_float, ctypes.c_int32]
        lib.fnn_argmax_s8.restype = ctypes.c_uint32
        lib.shim_conv_s8.argtypes = [ctypes.c_void_p] * 5 + [
            ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32] + [
            ctypes.c_int32] * 3 + [ctypes.c_uint32] + [ctypes.c_void_p] * 3
        lib.shim_dwconv_s8.argtypes = [ctypes.c_void_p] * 4 + [
            ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32] + [
            ctypes.c_int32] * 3 + [ctypes.c_void_p] * 2
        lib.shim_pool_s8.argtypes = [ctypes.c_void_p] * 2 + [ctypes.c_int] + [
            ctypes.c_int32] * 2 + [ctypes.c_void_p]
        lib.shim_mean_s8.argtypes = [ctypes.c_void_p] + [ctypes.c_uint32] * 3 + [
            ctypes.c_int32, ctypes.c_double, ctypes.c_int32, ctypes.c_void_p]
        lib.shim_add_s8.argtypes = [ctypes.c_void_p] * 2 + [
            ctypes.c_uint32, ctypes.c_double, ctypes.c_int32, ctypes.c_double,
            ctypes.c_int32, ctypes.c_double] + [ctypes.c_int32] * 3 + [
            ctypes.c_void_p]
        lib.fnn_concat_s8.argtypes = [ctypes.c_void_p] * 2 + [
            ctypes.c_uint32] * 2 + [ctypes.c_void_p]
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


def case_conv(libs, ih, iw, ic, oc, kh, kw, s, same, act, per_channel, seed,
              runs=20):
    """CONV_2D with random weights, in both weight layouts."""
    rng = np.random.default_rng(seed)
    s_in, s_out = 0.05, 0.25
    s_w = (rng.uniform(0.5, 1.5, oc) * 0.004 if per_channel
           else np.array([0.004])).astype(np.float32)
    zx, zy = int(rng.integers(-128, 128)), int(rng.integers(-128, 128))
    w = rng.integers(-127, 128, (oc, kh, kw, ic), dtype=np.int8)
    bias = rng.integers(-20000, 20000, oc, dtype=np.int32)
    if same:
        oh, ow = -(-ih // s), -(-iw // s)
        pt = max((oh - 1) * s + kh - ih, 0) // 2
        pl = max((ow - 1) * s + kw - iw, 0) // 2
    else:
        oh, ow, pt, pl = (ih - kh) // s + 1, (iw - kw) // s + 1, 0, 0
    m = tfl.Model()
    x = m.tensor("x", [1, ih, iw, ic], tfl.INT8, scale=s_in, zp=zx)
    wt = m.tensor("w", [oc, kh, kw, ic], tfl.INT8, scale=list(s_w),
                  zp=[0] * len(s_w), data=w)
    bscale = [float(np.float32(s_in) * v) for v in s_w]
    b = m.tensor("b", [oc], tfl.INT32, scale=bscale, zp=[0] * len(s_w), data=bias)
    y = m.tensor("y", [1, oh, ow, oc], tfl.INT8, scale=s_out, zp=zy)
    m.op("CONV_2D", [x, wt, b], [y], Padding=0 if same else 1, StrideW=s,
         StrideH=s, FusedActivationFunction=act)
    xs = [rng.integers(-128, 128, (1, ih, iw, ic), dtype=np.int8)
          for _ in range(runs)]
    got = tfl.run_reference(m.build(), xs)
    scales = np.array([v / float(np.float32(s_out)) for v in bscale])
    lo, hi = act_range(act, s_out, zy)
    dims = np.array([ih, iw, ic, oh, ow, oc, kh, kw, s, s, pt, pl], np.uint32)
    wc = np.ascontiguousarray(w)
    packed = np.zeros(w.size, np.int8)
    bf = np.zeros(oc, np.int32)
    scratch = np.zeros(kh * kw * ic, np.int8)
    out = np.zeros(oh * ow * oc, np.int8)
    bad = []
    for lib in libs:
        wrong = 0
        for use_packed in (0, 1):
            for xv, ref in zip(xs, got):
                xc = np.ascontiguousarray(xv.reshape(-1))
                out[:] = 0
                lib.shim_conv_s8(xc.ctypes.data, wc.ctypes.data, packed.ctypes.data,
                                 dims.ctypes.data, bias.ctypes.data, zx,
                                 scales.ctypes.data, len(scales), zy, lo, hi,
                                 use_packed, bf.ctypes.data, scratch.ctypes.data,
                                 out.ctypes.data)
                wrong += int(np.count_nonzero(out != ref.reshape(-1)))
        bad.append(wrong)
    name = (f"conv {kh}x{kw} stride {s}, {ih}x{iw}x{ic} -> {oh}x{ow}x{oc}"
            + (", SAME" if same else "") + {0: "", RELU: ", ReLU", RELU6: ", ReLU6"}[act]
            + (", per channel" if per_channel else ""))
    return name, 2 * runs * oh * ow * oc, bad


def case_dwconv(libs, ih, iw, ic, mult, kh, kw, s, same, act, per_channel, seed,
                runs=20):
    """DEPTHWISE_CONV_2D with random weights."""
    rng = np.random.default_rng(seed)
    oc = ic * mult
    s_in, s_out = 0.05, 0.1
    s_w = (rng.uniform(0.5, 1.5, oc) * 0.004 if per_channel
           else np.array([0.004])).astype(np.float32)
    zx, zy = int(rng.integers(-128, 128)), int(rng.integers(-128, 128))
    w = rng.integers(-127, 128, (1, kh, kw, oc), dtype=np.int8)
    bias = rng.integers(-2000, 2000, oc, dtype=np.int32)
    if same:
        oh, ow = -(-ih // s), -(-iw // s)
        pt = max((oh - 1) * s + kh - ih, 0) // 2
        pl = max((ow - 1) * s + kw - iw, 0) // 2
    else:
        oh, ow, pt, pl = (ih - kh) // s + 1, (iw - kw) // s + 1, 0, 0
    m = tfl.Model()
    x = m.tensor("x", [1, ih, iw, ic], tfl.INT8, scale=s_in, zp=zx)
    wt = m.tensor("w", [1, kh, kw, oc], tfl.INT8, scale=list(s_w),
                  zp=[0] * len(s_w), axis=3, data=w)
    bscale = [float(np.float32(s_in) * v) for v in s_w]
    b = m.tensor("b", [oc], tfl.INT32, scale=bscale, zp=[0] * len(s_w), data=bias)
    y = m.tensor("y", [1, oh, ow, oc], tfl.INT8, scale=s_out, zp=zy)
    m.op("DEPTHWISE_CONV_2D", [x, wt, b], [y], Padding=0 if same else 1,
         StrideW=s, StrideH=s, DepthMultiplier=mult, FusedActivationFunction=act)
    xs = [rng.integers(-128, 128, (1, ih, iw, ic), dtype=np.int8)
          for _ in range(runs)]
    got = tfl.run_reference(m.build(), xs)
    scales = np.array([v / float(np.float32(s_out)) for v in bscale])
    lo, hi = act_range(act, s_out, zy)
    dims = np.array([ih, iw, ic, oh, ow, oc, kh, kw, s, s, pt, pl], np.uint32)
    wc = np.ascontiguousarray(w.reshape(-1))
    bf = np.zeros(oc, np.int32)
    out = np.zeros(oh * ow * oc, np.int8)
    bad = []
    for lib in libs:
        wrong = 0
        for xv, ref in zip(xs, got):
            xc = np.ascontiguousarray(xv.reshape(-1))
            out[:] = 0
            lib.shim_dwconv_s8(xc.ctypes.data, wc.ctypes.data, dims.ctypes.data,
                               bias.ctypes.data, zx, scales.ctypes.data, len(scales),
                               zy, lo, hi, bf.ctypes.data, out.ctypes.data)
            wrong += int(np.count_nonzero(out != ref.reshape(-1)))
        bad.append(wrong)
    name = (f"depthwise {kh}x{kw} stride {s}, {ih}x{iw}x{ic} -> {oh}x{ow}x{oc}"
            + (", SAME" if same else "") + {0: "", RELU: ", ReLU", RELU6: ", ReLU6"}[act]
            + (", per channel" if per_channel else ""))
    return name, runs * oh * ow * oc, bad


def case_pool(libs, op, ih, iw, c, kh, kw, s, same, act, seed, runs=20):
    """MAX_POOL_2D or AVERAGE_POOL_2D on random inputs."""
    rng = np.random.default_rng(seed)
    scale, zp = 0.05, int(rng.integers(-128, 128))
    if same:
        oh, ow = -(-ih // s), -(-iw // s)
        pt = max((oh - 1) * s + kh - ih, 0) // 2
        pl = max((ow - 1) * s + kw - iw, 0) // 2
    else:
        oh, ow, pt, pl = (ih - kh) // s + 1, (iw - kw) // s + 1, 0, 0
    m = tfl.Model()
    x = m.tensor("x", [1, ih, iw, c], tfl.INT8, scale=scale, zp=zp)
    y = m.tensor("y", [1, oh, ow, c], tfl.INT8, scale=scale, zp=zp)
    m.op(op, [x], [y], Padding=0 if same else 1, StrideW=s, StrideH=s,
         FilterWidth=kw, FilterHeight=kh, FusedActivationFunction=act)
    xs = [rng.integers(-128, 128, (1, ih, iw, c), dtype=np.int8)
          for _ in range(runs)]
    got = tfl.run_reference(m.build(), xs)
    lo, hi = act_range(act, scale, zp)
    dims = np.array([ih, iw, c, oh, ow, kh, kw, s, s, pt, pl], np.uint32)
    out = np.zeros(oh * ow * c, np.int8)
    bad = []
    for lib in libs:
        wrong = 0
        for xv, ref in zip(xs, got):
            xc = np.ascontiguousarray(xv.reshape(-1))
            out[:] = 0
            lib.shim_pool_s8(xc.ctypes.data, dims.ctypes.data,
                             int(op == "AVERAGE_POOL_2D"), lo, hi, out.ctypes.data)
            wrong += int(np.count_nonzero(out != ref.reshape(-1)))
        bad.append(wrong)
    name = ({"MAX_POOL_2D": "max", "AVERAGE_POOL_2D": "average"}[op]
            + f" pool {kh}x{kw} stride {s}, {ih}x{iw}x{c} -> {oh}x{ow}x{c}"
            + (", SAME" if same else "") + {0: "", RELU: ", ReLU", RELU6: ", ReLU6"}[act])
    return name, runs * oh * ow * c, bad


def case_mean(libs, h, w, c, s_in, s_out, seed, runs=50):
    """MEAN over height and width, with keep_dims."""
    rng = np.random.default_rng(seed)
    zx, zy = int(rng.integers(-128, 128)), int(rng.integers(-128, 128))
    if s_in == s_out:
        zy = zx
    m = tfl.Model()
    x = m.tensor("x", [1, h, w, c], tfl.INT8, scale=s_in, zp=zx)
    axes = m.tensor("axes", [2], tfl.INT32, data=[1, 2])
    y = m.tensor("y", [1, 1, 1, c], tfl.INT8, scale=s_out, zp=zy)
    m.op("MEAN", [x, axes], [y])
    xs = [rng.integers(-128, 128, (1, h, w, c), dtype=np.int8)
          for _ in range(runs)]
    got = tfl.run_reference(m.build(), xs)
    scale = float(np.float32(s_in)) / float(np.float32(s_out))
    out = np.zeros(c, np.int8)
    bad = []
    for lib in libs:
        wrong = 0
        for xv, ref in zip(xs, got):
            xc = np.ascontiguousarray(xv.reshape(-1))
            out[:] = 0
            lib.shim_mean_s8(xc.ctypes.data, h, w, c, zx, scale, zy, out.ctypes.data)
            wrong += int(np.count_nonzero(out != ref.reshape(-1)))
        bad.append(wrong)
    name = f"mean of {h}x{w}x{c}, scales {s_in:g} / {s_out:g}"
    return name, runs * c, bad


def case_add(libs, shape, s1, s2, sy, act, seed, runs=20):
    """ADD of two tensors of the same shape."""
    rng = np.random.default_rng(seed)
    z1, z2, zy = (int(v) for v in rng.integers(-128, 128, 3))
    m = tfl.Model()
    a = m.tensor("a", shape, tfl.INT8, scale=s1, zp=z1)
    b = m.tensor("b", shape, tfl.INT8, scale=s2, zp=z2)
    y = m.tensor("y", shape, tfl.INT8, scale=sy, zp=zy)
    m.op("ADD", [a, b], [y], FusedActivationFunction=act)
    blob = m.build(inputs=[a, b], outputs=[y])
    feeds = [(rng.integers(-128, 128, shape, dtype=np.int8),
              rng.integers(-128, 128, shape, dtype=np.int8)) for _ in range(runs)]
    got = tfl.run_reference(blob, feeds)
    f = [float(np.float32(v)) for v in (s1, s2, sy)]
    lo, hi = act_range(act, sy, zy)
    n = int(np.prod(shape))
    out = np.zeros(n, np.int8)
    bad = []
    for lib in libs:
        wrong = 0
        for (xa, xb), ref in zip(feeds, got):
            ca, cb = (np.ascontiguousarray(v.reshape(-1)) for v in (xa, xb))
            out[:] = 0
            lib.shim_add_s8(ca.ctypes.data, cb.ctypes.data, n, f[0], z1, f[1], z2,
                            f[2], zy, lo, hi, out.ctypes.data)
            wrong += int(np.count_nonzero(out != ref.reshape(-1)))
        bad.append(wrong)
    name = (f"add {'x'.join(map(str, shape[1:]))}, scales {s1:g} + {s2:g} -> {sy:g}"
            + {0: "", RELU: ", ReLU", RELU6: ", ReLU6"}[act])
    return name, runs * n, bad


def case_concat(libs, shapes, axis, seed, runs=10):
    """CONCATENATION of tensors at one scale and zero point."""
    rng = np.random.default_rng(seed)
    zp = int(rng.integers(-128, 128))
    m = tfl.Model()
    ins = [m.tensor(f"x{i}", s, tfl.INT8, scale=0.05, zp=zp)
           for i, s in enumerate(shapes)]
    oshape = list(shapes[0])
    oshape[axis] = sum(s[axis] for s in shapes)
    y = m.tensor("y", oshape, tfl.INT8, scale=0.05, zp=zp)
    m.op("CONCATENATION", ins, [y], Axis=axis)
    blob = m.build(inputs=ins, outputs=[y])
    feeds = [tuple(rng.integers(-128, 128, s, dtype=np.int8) for s in shapes)
             for _ in range(runs)]
    got = tfl.run_reference(blob, feeds)
    outer = int(np.prod(oshape[:axis]))
    lens = (ctypes.c_uint32 * len(shapes))(*[int(np.prod(s[axis:])) for s in shapes])
    out = np.zeros(int(np.prod(oshape)), np.int8)
    bad = []
    for lib in libs:
        wrong = 0
        for xs, ref in zip(feeds, got):
            cs = [np.ascontiguousarray(v.reshape(-1)) for v in xs]
            ptrs = (ctypes.c_void_p * len(cs))(*[c.ctypes.data for c in cs])
            out[:] = 0
            lib.fnn_concat_s8(ptrs, lens, len(cs), outer, out.ctypes.data)
            wrong += int(np.count_nonzero(out != ref.reshape(-1)))
        bad.append(wrong)
    name = (f"concat {' + '.join('x'.join(map(str, s[1:])) for s in shapes)}"
            f" on axis {axis}")
    return name, runs * out.size, bad


# The reference models made only of fully connected layers: (inputs, outputs,
# fused activation) per layer, and whether a softmax ends the model.
MODELS = (
    ("R0, XOR MLP 2 -> 4 -> 1", ((2, 4, RELU), (4, 1, 0)), False),
    ("R1, MLP 64 -> 32 -> 16 -> 4, softmax",
     ((64, 32, RELU), (32, 16, RELU), (16, 4, 0)), True),
    ("R4, autoencoder 64 -> 32 -> 8 -> 32 -> 64",
     ((64, 32, RELU), (32, 8, RELU), (8, 32, RELU), (32, 64, 0)), False),
)


def build_model(layers, softmax, rng):
    """Writes a model of fully connected layers, and returns it with what the
    hand-built chain needs for each layer and the last layer's output scale."""
    f32 = np.float32
    m = tfl.Model()
    s_x, z_x = 0.05, int(rng.integers(-128, 128))
    x = m.tensor("x", [1, layers[0][0]], tfl.INT8, scale=s_x, zp=z_x)
    cur, plan = x, []
    for i, (n_in, n_out, act) in enumerate(layers):
        s_w = float(rng.uniform(0.002, 0.01))
        # an output scale that spreads the sums over most of int8
        s_y = s_x * s_w * 127.0 * math.sqrt(n_in) / 3.0
        z_y = -128 if act == RELU else int(rng.integers(-128, 128))
        w = rng.integers(-127, 128, (n_out, n_in), dtype=np.int8)
        bias = rng.integers(-2000, 2000, n_out, dtype=np.int32)
        wt = m.tensor(f"w{i}", [n_out, n_in], tfl.INT8, scale=s_w, zp=0, data=w)
        b = m.tensor(f"b{i}", [n_out], tfl.INT32,
                     scale=float(f32(s_x) * f32(s_w)), zp=0, data=bias)
        y = m.tensor(f"y{i}", [1, n_out], tfl.INT8, scale=s_y, zp=z_y)
        m.op("FULLY_CONNECTED", [cur, wt, b], [y], FusedActivationFunction=act)
        eff = float(f32(s_x) * f32(s_w)) / float(f32(s_y))
        plan.append((np.ascontiguousarray(w), bias, z_x, eff, z_y)
                    + act_range(act, s_y, z_y))
        cur, s_x, z_x = y, s_y, z_y
    if softmax:
        out_t = m.tensor("p", [1, layers[-1][1]], tfl.INT8, scale=1.0 / 256.0, zp=-128)
        m.op("SOFTMAX", [cur], [out_t], Beta=1.0)
        cur = out_t
    return m.build(inputs=[x], outputs=[cur]), plan, s_x


def case_model(libs, name, layers, softmax, seed, runs=500):
    """A whole model in the interpreter against flexnn's layers chained by hand,
    each layer's output the next one's input."""
    rng = np.random.default_rng(seed)
    f32 = np.float32
    blob, plan, s_x = build_model(layers, softmax, rng)
    xs = [rng.integers(-128, 128, (1, layers[0][0]), dtype=np.int8)
          for _ in range(runs)]
    got = tfl.run_reference(blob, xs)
    tab = np.zeros(256, np.uint32)
    bad = []
    for lib in libs:
        if softmax:
            assert lib.fnn_softmax_s8_build(1.0, float(f32(s_x)), tab.ctypes.data) == 0
        wrong = 0
        for k, (xv, ref) in enumerate(zip(xs, got)):
            a = np.ascontiguousarray(xv[0])
            for w, bias, zx, eff, zy, lo, hi in plan:
                n_out, n_in = w.shape
                out = np.zeros(n_out, np.int8)
                packed = np.zeros(n_in * n_out, np.int8)
                bf = np.zeros(n_out, np.int32)
                lib.shim_dense_s8(a.ctypes.data, w.ctypes.data, packed.ctypes.data,
                                  n_in, n_out, bias.ctypes.data, zx, eff, zy, lo, hi,
                                  k % 2, bf.ctypes.data, out.ctypes.data)
                a = out
            if softmax:
                p = np.zeros(a.size, np.int8)
                lib.fnn_softmax_s8(a.ctypes.data, p.ctypes.data, a.size, tab.ctypes.data)
                a = p
            wrong += int(np.count_nonzero(a != ref[0]))
        bad.append(wrong)
    return name, runs * layers[-1][1], bad


def case_converted(name, layers, softmax, seed, out_dir, runs=500):
    """The same model through fnn_tflite: the generated C, built in each
    rounding mode, against the interpreter."""
    blob, _, _ = build_model(layers, softmax, np.random.default_rng(seed))
    path = Path(out_dir) / f"r{seed}.tflite"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(blob)
    cname, _ = fnn_tflite.convert(path, out_dir)
    bad = [fnn_tflite.check(path, out_dir, cname, mode, runs=runs, seed=seed)[1]
           for mode in ("up", "away", "gemmlowp")]
    return name + ", converted", runs * layers[-1][1], bad


def case_reshape(out_dir, runs=500):
    """RESHAPE on both sides of a fully connected layer, as exporters write it:
    the converter turns each into an alias of the same bytes."""
    rng = np.random.default_rng(97)
    m = tfl.Model()
    x = m.tensor("x", [1, 4, 16], tfl.INT8, scale=0.05, zp=-3)
    s1 = m.tensor("s1", [2], tfl.INT32, data=[1, 64])
    flat = m.tensor("flat", [1, 64], tfl.INT8, scale=0.05, zp=-3)
    w = m.tensor("w", [8, 64], tfl.INT8, scale=0.004, zp=0,
                 data=rng.integers(-127, 128, (8, 64), dtype=np.int8))
    b = m.tensor("b", [8], tfl.INT32, scale=float(np.float32(0.05) * np.float32(0.004)),
                 zp=0, data=rng.integers(-2000, 2000, 8, dtype=np.int32))
    h = m.tensor("h", [1, 8], tfl.INT8, scale=0.02, zp=-128)
    s2 = m.tensor("s2", [3], tfl.INT32, data=[1, 2, 4])
    y = m.tensor("y", [1, 2, 4], tfl.INT8, scale=0.02, zp=-128)
    m.op("RESHAPE", [x, s1], [flat])
    m.op("FULLY_CONNECTED", [flat, w, b], [h], FusedActivationFunction=RELU)
    m.op("RESHAPE", [h, s2], [y])
    path = Path(out_dir) / "reshape.tflite"
    path.write_bytes(m.build(inputs=[x], outputs=[y]))
    cname, _ = fnn_tflite.convert(path, out_dir)
    bad = [fnn_tflite.check(path, out_dir, cname, mode, runs=runs, seed=97)[1]
           for mode in ("up", "away", "gemmlowp")]
    return "reshape -> fully connected 64 -> 8 -> reshape, converted", runs * 8, bad


def case_exported(path, out_dir, runs=500):
    """A model TensorFlow's converter wrote, through fnn_tflite."""
    cname, _ = fnn_tflite.convert(path, out_dir)
    n = 0
    bad = []
    for mode in ("up", "away", "gemmlowp"):
        n, b = fnn_tflite.check(path, out_dir, cname, mode, runs=runs, seed=98)
        bad.append(b)
    return f"{Path(path).name}, exported by TensorFlow, converted", n, bad


def case_converter_arith(lib, seed=99):
    """The converter's arithmetic against flexnn's own: multipliers against
    fnn_quant_from_scale(), softmax tables against fnn_softmax_s8_build(), and
    clamps against act_range(). Returns the number compared and differing."""
    rng = np.random.default_rng(seed)
    scales = list(rng.uniform(0.0, 1.0, 50000) * 2.0 ** rng.integers(-40, 40, 50000))
    scales += [2.0 ** e for e in range(-40, 40)] + [0.0, 1.0 - 2.0 ** -53, 0.5]
    n = bad = 0
    mult, rsh = ctypes.c_int32(), ctypes.c_int8()
    for sc in scales:
        lib.fnn_quant_from_scale(float(sc), ctypes.byref(mult), ctypes.byref(rsh))
        n += 1
        bad += fnn_tflite.quantize_multiplier(float(sc)) != (mult.value, rsh.value)
    tab = np.zeros(256, np.uint32)
    for _ in range(200):
        beta, s_x = float(rng.uniform(0.1, 3.0)), float(rng.uniform(0.001, 0.5))
        lib.fnn_softmax_s8_build(beta, s_x, tab.ctypes.data)
        n += 1
        bad += fnn_tflite.lower_softmax_table(beta * s_x) != [int(v) for v in tab]
    for _ in range(5000):
        act = int(rng.choice([0, RELU, 2, RELU6]))
        sc, zp = float(np.float32(rng.uniform(0.001, 0.2))), int(rng.integers(-128, 128))
        n += 1
        if act == 2:
            q = lambda f: zp + tfl_round(float(np.float32(f) / np.float32(sc)))
            want = (max(-128, q(-1.0)), min(127, q(1.0)))
        else:
            want = act_range(act, sc, zp)
        bad += fnn_tflite.act_range(act, sc, zp) != want
    return n, bad


def case_convert_errors(out_dir):
    """Models the converter must refuse, and the words its message must hold."""
    cases = []
    m = tfl.Model()
    x = m.tensor("x", [2, 3], tfl.INT8, scale=0.1, zp=0)
    perm = m.tensor("perm", [2], tfl.INT32, data=[1, 0])
    y = m.tensor("y", [3, 2], tfl.INT8, scale=0.1, zp=0)
    m.op("TRANSPOSE", [x, perm], [y])
    cases.append(("transpose", m.build(inputs=[x], outputs=[y]),
                  "op 0 (TRANSPOSE) is not supported"))
    m = tfl.Model()
    x = m.tensor("x", [1, 4], tfl.FLOAT32)
    y = m.tensor("y", [1, 4], tfl.INT8, scale=0.1, zp=0)
    m.op("QUANTIZE", [x], [y])
    cases.append(("float input", m.build(inputs=[x], outputs=[y]),
                  "input 'x' is FLOAT32"))
    m = tfl.Model()
    x = m.tensor("x", [1, 4], tfl.INT8, scale=0.1, zp=0)
    y = m.tensor("y", [1, 4], tfl.INT8, scale=0.1, zp=0)
    m.op("SOFTMAX", [x], [y], Beta=1.0)
    cases.append(("softmax output scale", m.build(inputs=[x], outputs=[y]),
                  "scale of 1/256"))
    wrong = []
    for name, blob, words in cases:
        path = Path(out_dir) / "refused.tflite"
        path.write_bytes(blob)
        try:
            fnn_tflite.convert(path, out_dir)
            wrong.append(f"{name}: converted")
        except fnn_tflite.ConvertError as e:
            if words not in str(e):
                wrong.append(f"{name}: {e}")
    return len(cases), wrong


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


def case_softmax(lib, n, s_in, zx, beta, seed, runs=200):
    """Softmax of n scores against TFLite's reference softmax."""
    m = tfl.Model()
    x = m.tensor("x", [1, n], tfl.INT8, scale=s_in, zp=zx)
    y = m.tensor("y", [1, n], tfl.INT8, scale=1.0 / 256.0, zp=-128)
    m.op("SOFTMAX", [x], [y], Beta=beta)
    rng = np.random.default_rng(seed)
    xs = [rng.integers(-128, 128, (1, n), dtype=np.int8) for _ in range(runs)]
    got = tfl.run_reference(m.build(), xs)
    tab = np.zeros(256, np.uint32)
    assert lib.fnn_softmax_s8_build(beta, s_in, tab.ctypes.data) == 0
    out = np.zeros(n, np.int8)
    differ, by = 0, 0
    for xv, want in zip(xs, got):
        xc = np.ascontiguousarray(xv[0])
        lib.fnn_softmax_s8(xc.ctypes.data, out.ctypes.data, n, tab.ctypes.data)
        d = np.abs(out.astype(int) - want[0].astype(int))
        differ += int(np.count_nonzero(d))
        by = max(by, int(d.max()))
    name = f"softmax of {n}, scale {s_in:.4g}, zero point {zx}, beta {beta:g}"
    return name, differ, by, runs * n


def case_argmax(lib, n, seed, runs=500):
    """ARG_MAX over n values, half the runs full of ties."""
    m = tfl.Model()
    x = m.tensor("x", [1, n], tfl.INT8, scale=0.1, zp=0)
    axis = m.tensor("axis", [1], tfl.INT32, data=[1])
    y = m.tensor("y", [1], tfl.INT32)
    m.op("ARG_MAX", [x, axis], [y])
    rng = np.random.default_rng(seed)
    xs = [rng.integers(-128, 128 if r % 2 else -124, (1, n), dtype=np.int8)
          for r in range(runs)]
    got = tfl.run_reference(m.build(), xs)
    wrong = 0
    for xv, want in zip(xs, got):
        xc = np.ascontiguousarray(xv[0])
        wrong += lib.fnn_argmax_s8(xc.ctypes.data, n) != int(want[0])
    return f"argmax of {n}", runs, wrong


def multiplier(lib, scale):
    mult, rsh = ctypes.c_int32(), ctypes.c_int8()
    assert lib.fnn_quant_from_scale(scale, ctypes.byref(mult), ctypes.byref(rsh)) == 0
    return mult.value, rsh.value


def case_quantize(libs, src, s, zp, seed, n=4096):
    """QUANTIZE from float or int16 (q15) to int8, in each rounding mode. It also
    counts the differences beyond the first 64 floats, which sit next to ties."""
    rng = np.random.default_rng(seed)
    if src == "float":
        xs = (rng.standard_normal(n) * 60.0 * s).astype(np.float32)
        xs[:64] = (np.arange(64, dtype=np.float32) - 32.0) * np.float32(0.5 * s)
        ttype, eff, fn = tfl.FLOAT32, 1.0 / float(np.float32(s)), "fnn_quantize_f32_s8"
        x = None
    else:
        xs = rng.integers(-32768, 32768, n, dtype=np.int16)
        ttype, eff, fn = tfl.INT16, 2.0**-15 / float(np.float32(s)), "fnn_quantize_q15_s8"
    m = tfl.Model()
    if src == "float":
        x = m.tensor("x", [1, n], ttype)
    else:
        x = m.tensor("x", [1, n], ttype, scale=2.0**-15, zp=0)
    y = m.tensor("y", [1, n], tfl.INT8, scale=s, zp=zp)
    m.op("QUANTIZE", [x], [y])
    want = tfl.run_reference(m.build(), [xs.reshape(1, n)])[0][0]
    out = np.zeros(n, np.int8)
    bad, far = [], []
    for lib in libs:
        mult, rsh = multiplier(lib, eff)
        getattr(lib, fn)(xs.ctypes.data, out.ctypes.data, n, mult, rsh, zp)
        bad.append(int(np.count_nonzero(out != want)))
        far.append(int(np.count_nonzero(out[64:] != want[64:])))
    return f"quantize {src} to int8, scale {s:.4g}, zero point {zp}", n, bad, far


def case_dequantize(lib, s, zp):
    """DEQUANTIZE every int8 to float, bit for bit."""
    xs = np.arange(-128, 128, dtype=np.int8)
    m = tfl.Model()
    x = m.tensor("x", [1, 256], tfl.INT8, scale=s, zp=zp)
    y = m.tensor("y", [1, 256], tfl.FLOAT32)
    m.op("DEQUANTIZE", [x], [y])
    want = tfl.run_reference(m.build(), [xs.reshape(1, 256)])[0][0]
    out = np.zeros(256, np.float32)
    lib.fnn_dequantize_s8_f32(xs.ctypes.data, out.ctypes.data, 256,
                              float(np.float32(s)), zp)
    wrong = int(np.count_nonzero(out.view(np.uint32) != want.view(np.uint32)))
    return f"dequantize int8 to float, scale {s:.4g}, zero point {zp}", 256, wrong


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
    for c in ((5, 5, 64, 64, 1, 1, 1, 0, 0, True, 31),
              (12, 12, 8, 16, 3, 3, 1, 1, RELU, True, 32),
              (1, 68, 16, 32, 1, 5, 1, 0, 0, False, 33),
              (7, 7, 3, 8, 3, 3, 2, 1, RELU6, True, 34),
              (3, 3, 2, 3, 5, 5, 1, 1, 0, False, 35)):
        name, n, bad = case_conv(libs, *c)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[GEMMLOWP] != 0
    for c in ((5, 5, 64, 1, 3, 3, 1, 1, 0, True, 41),
              (12, 12, 16, 1, 3, 3, 2, 1, RELU6, True, 42),
              (6, 6, 5, 3, 3, 3, 1, 0, RELU, True, 43),
              (1, 40, 8, 1, 1, 9, 1, 1, 0, False, 44)):
        name, n, bad = case_dwconv(libs, *c)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[GEMMLOWP] != 0
    for c in (("MAX_POOL_2D", 16, 16, 16, 2, 2, 2, False, 0, 51),
              ("MAX_POOL_2D", 7, 9, 5, 3, 3, 2, True, RELU, 52),
              ("MAX_POOL_2D", 6, 6, 8, 3, 3, 1, True, RELU6, 53),
              ("AVERAGE_POOL_2D", 5, 5, 64, 5, 5, 1, False, 0, 54),
              ("AVERAGE_POOL_2D", 8, 8, 4, 2, 2, 2, False, 0, 55),
              ("AVERAGE_POOL_2D", 7, 9, 5, 3, 3, 2, True, RELU, 56),
              ("AVERAGE_POOL_2D", 6, 6, 8, 3, 3, 1, True, RELU6, 57),
              ("AVERAGE_POOL_2D", 17, 17, 4, 17, 17, 1, False, 0, 58)):
        name, n, bad = case_pool(libs, *c)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= any(bad)
    for c in ((5, 5, 64, 0.05, 0.05, 61), (7, 7, 16, 0.1, 0.03, 62),
              (2, 2, 16, 0.05, 0.05, 63), (1, 49, 10, 0.02, 0.0625, 64),
              (20, 20, 8, 0.05, 0.05, 65)):
        name, n, bad = case_mean(libs, *c)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[GEMMLOWP] != 0
    for c in (([1, 32], 0.05, 0.05, 0.05, 0, 71), ([1, 4, 4, 16], 0.1, 0.03, 0.12, 0, 72),
              ([1, 8, 8, 8], 0.02, 0.5, 0.4, RELU, 73), ([1, 64], 0.25, 0.125, 1.0, 0, 74),
              ([1, 6, 6, 4], 0.07, 0.07, 0.05, RELU6, 75)):
        name, n, bad = case_add(libs, *c)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[GEMMLOWP] != 0
    for c in (([[1, 4, 4, 8], [1, 4, 4, 16]], 3, 81),
              ([[1, 2, 5, 3], [1, 2, 5, 1], [1, 2, 5, 6]], 3, 82),
              ([[1, 3, 4, 8], [1, 5, 4, 8]], 1, 83),
              ([[1, 4, 3, 2], [1, 4, 7, 2]], 2, 84)):
        name, n, bad = case_concat(libs, *c)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= any(bad)
    for i, c in enumerate(MODELS):
        name, n, bad = case_model(libs, *c, 90 + i)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[AWAY] != 0
        name, n, bad = case_converted(*c, 90 + i, Path(sys.argv[1] if len(sys.argv) > 1
                                                       else "build/nn-check") / "models")
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[AWAY] != 0
    models = Path(sys.argv[1] if len(sys.argv) > 1 else "build/nn-check") / "models"
    name, n, bad = case_reshape(models)
    print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
    failed |= bad[AWAY] != 0
    for path in sorted((Path(__file__).parent / "models").glob("*.tflite")):
        name, n, bad = case_exported(path, models)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[AWAY] != 0
    n, bad = case_converter_arith(libs[AWAY])
    print(f"{'converter multipliers, softmax tables and clamps':58} {n:8}  {bad:8} differ")
    failed |= bad != 0
    n, wrong = case_convert_errors(models)
    print(f"{'models the converter must refuse':58} {n:8}  {len(wrong):8} converted or wrongly worded")
    for w in wrong:
        print("  " + w)
    failed |= bool(wrong)
    print("\nmismatches per rounding mode: fully connected layers must match"
          "\nFNN_ROUND_AWAY, as must the models built from them; convolutions, means"
          "\nand adds FNN_ROUND_GEMMLOWP, the interpreter's rules; pools and"
          "\nconcatenations must match in every mode\n")

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

    print(f"\n{'softmax':58} {'differ':>6} {'by':>3}  {'outputs':>8}")
    for c in ((12, 1.0 / 16.0, 0, 1.0, 11), (10, 0.1, -30, 1.0, 12),
              (12, 0.25, 5, 0.5, 13), (256, 0.05, 0, 1.0, 14)):
        name, differ, by, n = case_softmax(libs[AWAY], *c)
        print(f"{name:58} {differ:6} {by:3}  {n:8}")
        failed |= differ != 0
    print("\noutputs that differ from TFLite's softmax, and by how much: none may\n")

    print(f"{'case':58} {'outputs':>8}  " + "  ".join(f"{m[10:]:>8}" for m in MODES))
    for c in (("int16", 0.0371, 3, 21), ("int16", 1.0 / 256.0, -128, 22),
              ("float", 0.0371, 3, 23), ("float", 0.5, -10, 24), ("float", 0.003, 0, 25)):
        name, n, bad, far = case_quantize(libs, *c)
        print(f"{name:58} {n:8}  " + "  ".join(f"{b:8}" for b in bad))
        failed |= bad[GEMMLOWP] != 0 if c[0] == "int16" else far[AWAY] != 0
    for s_, zp in ((0.0371, 3), (1.0 / 256.0, -128), (1e-30, 7), (3.0e35, 0)):
        name, n, wrong = case_dequantize(libs[AWAY], s_, zp)
        print(f"{name:58} {n:8}  {wrong:8}")
        failed |= wrong != 0
    print("\nfrom int16, FNN_ROUND_GEMMLOWP must match. From float, FNN_ROUND_AWAY must"
          "\nmatch beyond the 64 values next to ties, where TFLite rounds twice. To float,"
          "\nnothing may differ\n")

    print(f"{'argmax':58} {'runs':>6} {'wrong':>6}")
    for n, seed in ((1, 15), (12, 16), (256, 17)):
        name, runs, wrong = case_argmax(libs[AWAY], n, seed)
        print(f"{name:58} {runs:6} {wrong:6}")
        failed |= wrong != 0
    if failed:
        sys.exit("nn-check: flexnn differs from the TFLite reference kernels")
    print("nn-check: flexnn matches the TFLite reference kernels")


if __name__ == "__main__":
    main()
