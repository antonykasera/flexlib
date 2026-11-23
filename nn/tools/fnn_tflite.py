#!/usr/bin/env python3
"""Turns a full-integer int8 TFLite model into C that runs it with flexnn.

    python3 fnn_tflite.py model.tflite -o out/ [--name kws] [--check]

The output is <name>.h and <name>.c: the weights as constant arrays, one
flexnn layer per operator, and <name>_run(in, out, arena). --check builds the
C on this machine and compares it with the TFLite interpreter, which needs
ai-edge-litert and a C compiler.

Copyright (c) 2026 Antony Kasera
SPDX-License-Identifier: Apache-2.0
"""

import argparse
import importlib
import math
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
LAYOUT_VERSION = 1


def _mod(name):
    # `import tflite.Model` gives the class, which hides the module
    return importlib.import_module("tflite." + name)


class ConvertError(Exception):
    pass



_TYPES = {}
_OPS = {}


def _names():
    if not _TYPES:
        for cls, table in ((_mod("TensorType").TensorType, _TYPES),
                           (_mod("BuiltinOperator").BuiltinOperator, _OPS)):
            for k, v in vars(cls).items():
                if not k.startswith("_"):
                    table[v] = k


class Tensor:
    def __init__(self, idx, name, shape, dtype, scale, zp, axis, data):
        self.idx, self.name, self.shape, self.dtype = idx, name, shape, dtype
        self.scale, self.zp, self.axis, self.data = scale, zp, axis, data

    def size(self):
        return int(np.prod(self.shape)) if self.shape else 1


class Op:
    def __init__(self, idx, name, inputs, outputs, options):
        self.idx, self.name = idx, name
        self.inputs, self.outputs, self.options = inputs, outputs, options


_NP = {"INT8": np.int8, "INT16": np.int16, "INT32": np.int32,
       "FLOAT32": np.float32, "UINT8": np.uint8, "INT64": np.int64}


def load(path):
    """Reads a .tflite file into its tensors, operators, inputs and outputs."""
    _names()
    raw = Path(path).read_bytes()
    model = _mod("Model").Model.GetRootAs(raw, 0)
    if model.SubgraphsLength() != 1:
        raise ConvertError(f"{path}: {model.SubgraphsLength()} subgraphs; "
                           "flexnn takes models with one")
    g = model.Subgraphs(0)
    tensors = []
    for i in range(g.TensorsLength()):
        t = g.Tensors(i)
        dtype = _TYPES.get(t.Type(), str(t.Type()))
        q = t.Quantization()
        scale = list(q.ScaleAsNumpy()) if q is not None and q.ScaleLength() else []
        zp = [int(v) for v in q.ZeroPointAsNumpy()] if q is not None and q.ZeroPointLength() else []
        axis = q.QuantizedDimension() if q is not None else 0
        buf = model.Buffers(t.Buffer())
        data = None
        shape = [int(d) for d in t.ShapeAsNumpy()] if t.ShapeLength() else []
        if buf is not None and buf.DataLength() > 0 and dtype in _NP:
            data = buf.DataAsNumpy().view(_NP[dtype]).reshape(shape)
        tensors.append(Tensor(i, t.Name().decode(), shape, dtype,
                              [float(np.float32(s)) for s in scale], zp, axis, data))
    ops = []
    for i in range(g.OperatorsLength()):
        o = g.Operators(i)
        code = model.OperatorCodes(o.OpcodeIndex())
        c = max(code.BuiltinCode(), code.DeprecatedBuiltinCode())
        name = _OPS.get(c, f"operator {c}")
        ops.append(Op(i, name, [int(v) for v in o.InputsAsNumpy()],
                      [int(v) for v in o.OutputsAsNumpy()], o.BuiltinOptions()))
    return (tensors, ops, [int(v) for v in g.InputsAsNumpy()],
            [int(v) for v in g.OutputsAsNumpy()])


def _options(op, cls):
    m = getattr(_mod(cls), cls)()
    if op.options is None:
        raise ConvertError(f"op {op.idx} ({op.name}): its options are missing")
    m.Init(op.options.Bytes, op.options.Pos)
    return m



def quantize_multiplier(scale):
    """TFLite's QuantizeMultiplier, in flexnn's (mult, rsh) form, as
    fnn_quant_from_scale() computes it."""
    if scale == 0.0:
        return 0, -1
    q, e = math.frexp(scale)
    m = math.floor(q * 2147483648.0 + 0.5)
    if m == 1 << 31:
        m //= 2
        e += 1
    if e < -31:
        m, e = 0, 0
    elif e > 30:
        m, e = (1 << 31) - 1, 30
    return int(m), -1 - e


NONE, RELU, RELU_N1_TO_1, RELU6 = 0, 1, 2, 3


def act_range(act, scale, zp):
    """TFLite's CalculateActivationRangeQuantized for int8, in float32."""
    def quant(f):
        v = float(np.float32(f) / np.float32(scale))
        return zp + int(math.floor(abs(v) + 0.5)) * (1 if v >= 0 else -1)
    lo, hi = -128, 127
    if act == RELU:
        lo = max(lo, quant(0.0))
    elif act == RELU_N1_TO_1:
        lo, hi = max(lo, quant(-1.0)), min(hi, quant(1.0))
    elif act == RELU6:
        lo, hi = max(lo, quant(0.0)), min(hi, quant(6.0))
    elif act != NONE:
        raise ConvertError(f"fused activation {act} is not supported")
    return lo, hi


def wrap32(v):
    return int((int(v) + (1 << 31)) % (1 << 32) - (1 << 31))


def mean_quant(scale, n):
    """TFLite's multiplier for scale divided by n, as fnn_mean_quant()
    computes it."""
    m, r = quantize_multiplier(scale)
    sh = min(n.bit_length() - 1, 30 - r)
    return (m << sh) // n, r + sh


def add_quant(s1, s2, sy):
    """Returns fnn_add_init()'s multipliers and shifts, first for the gemmlowp
    rule and then for the other modes."""
    big = max(s1, s2)
    twice = 2.0 * big
    g = [quantize_multiplier(v) for v in (s1 / twice, s2 / twice,
                                          twice / (1048576.0 * sy))]
    gm, gs = [m for m, _ in g], [r + 1 for _, r in g]
    m1, m2 = s1 / sy, s2 / sy
    mb, r = quantize_multiplier(max(m1, m2))
    ms = int(min(m1, m2) * math.ldexp(1.0, 32 + r) + 0.5)
    pair = [mb, ms] if s1 > s2 else [ms, mb]
    if r > 22:
        pair, r = [0, 0], 0
    return (gm, gs), (pair + [0], [-r if r < 0 else 0, r if r > 0 else 0, 0])


def lut_table(f, s_x, zx, s_y, zy):
    """fnn_lut_s8_build()'s table for f, indexed by the input's bit pattern."""
    tab = [0] * 256
    for q in range(-128, 128):
        v = f(s_x * (q - zx)) / s_y + zy
        v = -128.0 if not (v >= -128.0) else (127.0 if v > 127.0 else v)
        tab[q & 255] = int(v + 128.5) - 128
    return tab


class Layer:
    """One flexnn call, with the tensors it reads and writes, its constants as
    C, and the call text in which {x0}, {x1}, {y} and {s} stand for its
    buffers."""

    def __init__(self, op, kind, reads, writes, decls="", call="", flash=0,
                 macs=0, scratch=0, in_place=(), outer=0, packed=False, geom=None):
        self.op, self.kind, self.reads, self.writes = op, kind, reads, writes
        self.decls, self.call, self.flash = decls, call, flash
        self.macs, self.scratch = macs, scratch
        self.in_place, self.outer, self.packed = in_place, outer, packed
        self.geom, self.ops = geom, (op,)


def pack_rows(w):
    """The rows of w in the order fnn_dense_pack_s8() writes."""
    n_out = w.shape[0]
    parts, o = [], 0
    while n_out - o >= 8:
        parts.append(w[o:o + 8].T.reshape(-1))
        o += 8
    if n_out - o >= 4:
        parts.append(w[o:o + 4].T.reshape(-1))
        o += 4
    parts.append(w[o:].reshape(-1))
    return np.concatenate(parts).tolist()


def _int8(op, t, what):
    if t.dtype != "INT8":
        raise ConvertError(f"op {op.idx} ({op.name}): {what} '{t.name}' is "
                           f"{t.dtype}; flexnn takes int8")
    if len(t.scale) < 1 or len(t.zp) < 1:
        raise ConvertError(f"op {op.idx} ({op.name}): {what} '{t.name}' is not "
                           "quantised")


def _err(op, text):
    return ConvertError(f"op {op.idx} ({op.name}): {text}")


def _quant(n, mult, rsh):
    return (_array("int32_t", f"m{n}", mult, per_line=8) +
            _array("int8_t", f"r{n}", rsh))


def _channel_quant(op, x, w, y, n_out):
    """A float32 product of the scales rounds some multipliers differently."""
    if any(z != 0 for z in w.zp):
        raise _err(op, "weights must have zero points of 0")
    s_w = w.scale if len(w.scale) == n_out else w.scale[:1]
    q = [quantize_multiplier(x.scale[0] * s / y.scale[0]) for s in s_w]
    return [m for m, _ in q], [r for _, r in q]


def _bias(op, T, n_out):
    if len(op.inputs) < 3 or op.inputs[2] < 0:
        return np.zeros(n_out, np.int64)
    b = T[op.inputs[2]]
    if b.dtype != "INT32" or b.data is None:
        raise _err(op, f"the bias is {b.dtype}, not a constant int32 tensor")
    return b.data.reshape(-1).astype(np.int64)


def _nhwc(op, t, what):
    if len(t.shape) != 4 or t.shape[0] != 1:
        raise _err(op, f"the {what} must be one image, [1, h, w, c]")
    return t.shape[1], t.shape[2], t.shape[3]


def _pad(size_in, size_out, k, s, padding):
    """TFLite's padding before the first row or column, which for SAME is half
    of what the windows overhang, rounded down, and for VALID none."""
    if padding == 0:
        return max((size_out - 1) * s + k - size_in, 0) // 2
    return 0


def _fits(size_in, size_out, k, s, p):
    """What fnn_conv_check() and the other checks ask of one dimension."""
    return (0 < size_in and 0 < size_out and 0 < k and 0 < s and p < k and
            (size_out - 1) * s < size_in + p)


def lower_fully_connected(op, T, ctx):
    x, w, y = T[op.inputs[0]], T[op.inputs[1]], T[op.outputs[0]]
    for t, what in ((x, "input"), (w, "weights"), (y, "output")):
        _int8(op, t, what)
    o = _options(op, "FullyConnectedOptions")
    if o.WeightsFormat() != 0:
        raise _err(op, "shuffled weights are not supported")
    if w.data is None or len(w.shape) != 2:
        raise _err(op, "the weights must be a constant [outputs][inputs] tensor")
    n_out, n_in = w.shape
    if x.size() != n_in:
        raise _err(op, f"{x.size()} inputs for weights of {n_in}; batches are "
                       "not supported")
    bias = _bias(op, T, n_out)
    zx, zy = x.zp[0], y.zp[0]
    wsum = w.data.astype(np.int64).sum(axis=1)
    folded = [wrap32(bias[c] - zx * wsum[c]) for c in range(n_out)]
    mult, rsh = _channel_quant(op, x, w, y, n_out)
    lo, hi = act_range(o.FusedActivationFunction(), y.scale[0], zy)
    n = op.idx
    decls = (_array("int8_t", f"w{n}", pack_rows(w.data), align=True) +
             _array("int32_t", f"b{n}", folded, per_line=8) + _quant(n, mult, rsh) +
             f"static const FNN_Dense l{n} = {{w{n}, b{n}, {n_in}u, {n_out}u, 1u, "
             f"{{m{n}, r{n}, {len(mult)}u}}, {{{zy}, {lo}, {hi}}}}};\n")
    return Layer(op, "dense", [x.idx], [y.idx], decls,
                 f"fnn_dense_s8(&l{n}, {{x0}}, {{y}});",
                 w.data.size + 9 * n_out, n_in * n_out, packed=True)


def _window(op, x_t, y_t, k_h, k_w, o, ctx):
    """Returns the input, padding and output of a windowed layer, counting a
    PAD just before it as the layer's own padding."""
    src = ctx["pad"].get(x_t.idx)
    xin = ctx["T"][src[0]] if src else x_t
    h, w, c = _nhwc(op, xin, "input")
    oh, ow, oc = _nhwc(op, y_t, "output")
    sh, sw = o.StrideH(), o.StrideW()
    if src:
        if o.Padding() != 1:
            raise _err(op, "a PAD before a SAME window is not supported")
        pt, pl = src[1], src[2]
    else:
        ph, pw = _nhwc(op, x_t, "input")[:2]
        pt = _pad(ph, oh, k_h, sh, o.Padding())
        pl = _pad(pw, ow, k_w, sw, o.Padding())
    if not (_fits(h, oh, k_h, sh, pt) and _fits(w, ow, k_w, sw, pl)):
        raise _err(op, "the window, stride and padding do not fit the shapes")
    return xin, (h, w, c), (oh, ow, oc), (sh, sw, pt, pl)


def _no_dilation(op, o):
    if o.DilationHFactor() != 1 or o.DilationWFactor() != 1:
        raise _err(op, "dilated windows are not supported")


def lower_conv(op, T, ctx):
    x, w, y = T[op.inputs[0]], T[op.inputs[1]], T[op.outputs[0]]
    for t, what in ((x, "input"), (w, "weights"), (y, "output")):
        _int8(op, t, what)
    o = _options(op, "Conv2DOptions")
    _no_dilation(op, o)
    if w.data is None or len(w.shape) != 4:
        raise _err(op, "the filter must be a constant [out][h][w][in] tensor")
    oc, kh, kw, ic = w.shape
    xin, (h, iw, c), (oh, ow, oc2), (sh, sw, pt, pl) = _window(op, x, y, kh, kw, o, ctx)
    if c != ic or oc2 != oc:
        raise _err(op, "the filter's channels do not match the tensors'")
    bias = _bias(op, T, oc)
    zx, zy = xin.zp[0], y.zp[0]
    wsum = w.data.astype(np.int64).reshape(oc, -1).sum(axis=1)
    folded = [wrap32(bias[j] - zx * wsum[j]) for j in range(oc)]
    mult, rsh = _channel_quant(op, xin, w, y, oc)
    lo, hi = act_range(o.FusedActivationFunction(), y.scale[0], zy)
    n = op.idx
    decls = (_array("int8_t", f"w{n}", pack_rows(w.data.reshape(oc, -1)), align=True) +
             _array("int32_t", f"b{n}", folded, per_line=8) + _quant(n, mult, rsh) +
             f"static const FNN_Conv l{n} = {{w{n}, b{n}, {{{h}u, {iw}u, {c}u}}, "
             f"{{{oh}u, {ow}u, {oc}u}}, {kh}u, {kw}u, {sh}u, {sw}u, {pt}u, {pl}u, "
             f"1u, {zx}, {{m{n}, r{n}, {len(mult)}u}}, {{{zy}, {lo}, {hi}}}}};\n")
    return Layer(op, "conv", [xin.idx], [y.idx], decls,
                 f"fnn_conv_s8(&l{n}, {{x0}}, {{y}}, {{s}});", w.data.size + 9 * oc,
                 oh * ow * oc * kh * kw * c, scratch=kh * kw * c, packed=True,
                 geom=(h, iw, c, oh, ow, oc, kh, kw, sh, sw, pt, pl))


def lower_depthwise(op, T, ctx):
    x, w, y = T[op.inputs[0]], T[op.inputs[1]], T[op.outputs[0]]
    for t, what in ((x, "input"), (w, "weights"), (y, "output")):
        _int8(op, t, what)
    o = _options(op, "DepthwiseConv2DOptions")
    _no_dilation(op, o)
    if w.data is None or len(w.shape) != 4 or w.shape[0] != 1:
        raise _err(op, "the filter must be a constant [1][h][w][out] tensor")
    _, kh, kw, oc = w.shape
    xin, (h, iw, c), (oh, ow, oc2), (sh, sw, pt, pl) = _window(op, x, y, kh, kw, o, ctx)
    if oc2 != oc or oc % c != 0:
        raise _err(op, "the output channels must be a multiple of the input's")
    bias = _bias(op, T, oc)
    zx, zy = xin.zp[0], y.zp[0]
    wsum = w.data.astype(np.int64).reshape(-1, oc).sum(axis=0)
    folded = [wrap32(bias[j] - zx * wsum[j]) for j in range(oc)]
    mult, rsh = _channel_quant(op, xin, w, y, oc)
    lo, hi = act_range(o.FusedActivationFunction(), y.scale[0], zy)
    n = op.idx
    decls = (_array("int8_t", f"w{n}", w.data.reshape(-1).tolist()) +
             _array("int32_t", f"b{n}", folded, per_line=8) + _quant(n, mult, rsh) +
             f"static const FNN_DwConv l{n} = {{w{n}, b{n}, {{{h}u, {iw}u, {c}u}}, "
             f"{{{oh}u, {ow}u, {oc}u}}, {kh}u, {kw}u, {sh}u, {sw}u, {pt}u, {pl}u, "
             f"{zx}, {{m{n}, r{n}, {len(mult)}u}}, {{{zy}, {lo}, {hi}}}}};\n")
    return Layer(op, "depthwise", [xin.idx], [y.idx], decls,
                 f"fnn_dwconv_s8(&l{n}, {{x0}}, {{y}});", w.data.size + 9 * oc,
                 oh * ow * oc * kh * kw, geom=(h, iw, c, oh, ow, oc, kh, kw, sh, sw, pt, pl))


def _same_quant(op, a, b):
    if a.scale[0] != b.scale[0] or a.zp[0] != b.zp[0]:
        raise _err(op, f"'{b.name}' must have the scale and zero point of "
                       f"'{a.name}'")


def _lower_pool(op, T, ctx, fn):
    x, y = T[op.inputs[0]], T[op.outputs[0]]
    _int8(op, x, "input")
    _int8(op, y, "output")
    _same_quant(op, x, y)
    o = _options(op, "Pool2DOptions")
    kh, kw = o.FilterHeight(), o.FilterWidth()
    _, (h, w, c), (oh, ow, oc), (sh, sw, pt, pl) = _window(op, x, y, kh, kw, o, ctx)
    if oc != c:
        raise _err(op, "the output must have the input's channels")
    lo, hi = act_range(o.FusedActivationFunction(), y.scale[0], y.zp[0])
    n = op.idx
    decls = (f"static const FNN_Pool l{n} = {{{{{h}u, {w}u, {c}u}}, "
             f"{{{oh}u, {ow}u, {c}u}}, {kh}u, {kw}u, {sh}u, {sw}u, {pt}u, {pl}u, "
             f"{lo}, {hi}}};\n")
    return Layer(op, "pool", [x.idx], [y.idx], decls,
                 f"{fn}(&l{n}, {{x0}}, {{y}});", 0, 0,
                 in_place=(0,) if pt == 0 and pl == 0 else ())


def lower_max_pool(op, T, ctx):
    return _lower_pool(op, T, ctx, "fnn_maxpool_s8")


def lower_average_pool(op, T, ctx):
    return _lower_pool(op, T, ctx, "fnn_avgpool_s8")


def lower_mean(op, T, ctx):
    x, y = T[op.inputs[0]], T[op.outputs[0]]
    _int8(op, x, "input")
    _int8(op, y, "output")
    h, w, c = _nhwc(op, x, "input")
    axes = ctx["v"].get(op.inputs[1])
    if axes is None or sorted(int(a) % 4 for a in np.atleast_1d(axes)) != [1, 2]:
        raise _err(op, "only a mean over height and width is supported")
    if y.size() != c:
        raise _err(op, "the output must have one value per channel")
    mult, rsh = mean_quant(float(np.float32(x.scale[0])) / float(np.float32(y.scale[0])),
                           h * w)
    n = op.idx
    decls = (f"static const FNN_Mean l{n} = {{{{{h}u, {w}u, {c}u}}, {x.zp[0]}, "
             f"{mult}, {rsh}, {{{y.zp[0]}, -128, 127}}}};\n")
    return Layer(op, "mean", [x.idx], [y.idx], decls,
                 f"fnn_mean_s8(&l{n}, {{x0}}, {{y}});", 0, 0)


def lower_add(op, T, ctx):
    a, b, y = T[op.inputs[0]], T[op.inputs[1]], T[op.outputs[0]]
    for t, what in ((a, "first input"), (b, "second input"), (y, "output")):
        _int8(op, t, what)
    if a.shape != b.shape or a.size() != y.size():
        raise _err(op, "the inputs must have the same shape; broadcasting is "
                       "not supported")
    s1, s2, sy = a.scale[0], b.scale[0], y.scale[0]
    if max(s1, s2) / sy >= 524288.0:
        raise _err(op, "the output scale is too small for the inputs'")
    lo, hi = act_range(_options(op, "AddOptions").FusedActivationFunction(), sy, y.zp[0])
    (gm, gs), (um, us) = add_quant(s1, s2, sy)
    n = op.idx

    def init(m, s):
        return (f"static const FNN_Add l{n} = {{{a.zp[0]}, {b.zp[0]}, "
                f"{{{m[0]}, {m[1]}, {m[2]}}}, {{{s[0]}, {s[1]}, {s[2]}}}, "
                f"{{{y.zp[0]}, {lo}, {hi}}}}};\n")
    decls = ("#if FNN_ROUNDING == FNN_ROUND_GEMMLOWP || "
             "FNN_ROUNDING == FNN_ROUND_TFLITE\n" + init(gm, gs) + "#else\n" +
             init(um, us) + "#endif\n")
    return Layer(op, "add", [a.idx, b.idx], [y.idx], decls,
                 f"fnn_add_s8(&l{n}, {{x0}}, {{x1}}, {{y}}, {y.size()}u);", 0, 0,
                 in_place=(0, 1))


def lower_concatenation(op, T, ctx):
    xs, y = [T[i] for i in op.inputs], T[op.outputs[0]]
    _int8(op, y, "output")
    o = _options(op, "ConcatenationOptions")
    if o.FusedActivationFunction() != 0:
        raise _err(op, "a fused activation on a concatenation is not supported")
    for t in xs:
        _int8(op, t, "input")
        _same_quant(op, y, t)
    axis = o.Axis() % len(y.shape)
    outer = int(np.prod(y.shape[:axis]))
    lens = [int(np.prod(t.shape[axis:])) for t in xs]
    n = op.idx
    decls = _array("uint32_t", f"c{n}", lens)
    ptrs = ", ".join(f"{{x{i}}}" for i in range(len(xs)))
    call = ("{{\n    const int8_t *const v[" + str(len(xs)) + "] = {{" + ptrs + "}};\n"
            f"    fnn_concat_s8(v, c{n}, {len(xs)}u, {outer}u, {{y}});\n  }}}}")
    return Layer(op, "concat", [t.idx for t in xs], [y.idx], decls, call, 0, 0,
                 outer=outer)


def logistic(v):
    return 1.0 / (1.0 + math.exp(-v)) if v > -700.0 else 0.0


UNARY = {"LOGISTIC": logistic,
         "TANH": math.tanh,
         "HARD_SWISH": lambda v: v * min(max(v + 3.0, 0.0), 6.0) / 6.0,
         "RELU": lambda v: max(v, 0.0),
         "RELU6": lambda v: min(max(v, 0.0), 6.0),
         "RELU_N1_TO_1": lambda v: min(max(v, -1.0), 1.0)}


def lower_unary(op, T, ctx):
    x, y = T[op.inputs[0]], T[op.outputs[0]]
    _int8(op, x, "input")
    _int8(op, y, "output")
    alpha = None
    if op.name == "LEAKY_RELU":
        alpha = float(np.float32(_options(op, "LeakyReluOptions").Alpha()))
        f = lambda v: v if v >= 0.0 else alpha * v  # noqa: E731
    else:
        f = UNARY[op.name]
    if ctx["tflite_tables"]:
        tab = interpreter_table(op.name, x.scale[0], x.zp[0], y.scale[0], y.zp[0], alpha)
    else:
        tab = lut_table(f, x.scale[0], x.zp[0], y.scale[0], y.zp[0])
    n = op.idx
    return Layer(op, "table", [x.idx], [y.idx], _array("int8_t", f"t{n}", tab),
                 f"fnn_lut_s8({{x0}}, {{y}}, {x.size()}u, t{n});", 256, 0,
                 in_place=(0,))


def lower_softmax(op, T, ctx):
    x, y = T[op.inputs[0]], T[op.outputs[0]]
    _int8(op, x, "input")
    _int8(op, y, "output")
    if y.zp[0] != -128 or y.scale[0] != float(np.float32(1.0 / 256.0)):
        raise _err(op, "the output must have TFLite's scale of 1/256 and zero "
                       "point of -128")
    beta = _options(op, "SoftmaxOptions").Beta()
    n_in = x.shape[-1] if x.shape else 1
    if x.size() != n_in:
        raise _err(op, "softmax over more than one row is not supported")
    k = float(beta) * x.scale[0]
    if not (k > 0.0) or not math.isfinite(k):
        raise _err(op, "beta times the input scale must be positive")
    n = op.idx
    return Layer(op, "softmax", [x.idx], [y.idx],
                 _array("uint32_t", f"e{n}", lower_softmax_table(k), per_line=6),
                 f"fnn_softmax_s8({{x0}}, {{y}}, {n_in}u, e{n});", 1024, 0)


def lower_softmax_table(k):
    """The exponentials fnn_softmax_s8_build() writes, for beta times the
    input scale k."""
    return [int(math.ldexp(math.exp(-k * d), 31) + 0.5) for d in range(256)]


def lower_reshape(op, T, ctx):
    x, y = T[op.inputs[0]], T[op.outputs[0]]
    if x.size() != y.size() or x.dtype != y.dtype:
        raise _err(op, "input and output differ in size")
    return Layer(op, "alias", [x.idx], [y.idx])


def interpreter_table(opname, s_x, zx, s_y, zy, alpha):
    """The table the TFLite interpreter's own kernel gives, read by running a
    one-operator model on all 256 inputs."""
    import flatbuffers
    from ai_edge_litert import interpreter as litert
    B = {n: getattr(_mod(n), n) for n in ("Model", "SubGraph", "Tensor", "Operator",
                                          "OperatorCode", "Buffer",
                                          "QuantizationParameters")}
    b = flatbuffers.Builder(1024)

    def vec(start, items, prepend):
        start(b, len(items))
        for v in reversed(items):
            prepend(v)
        return b.EndVector()
    _mod("Buffer").BufferStart(b)
    buf0 = _mod("Buffer").BufferEnd(b)
    tens = []
    for name, s, z in (("x", s_x, zx), ("y", s_y, zy)):
        nm = b.CreateString(name)
        sh = vec(_mod("Tensor").TensorStartShapeVector, [1, 256], b.PrependInt32)
        QP = _mod("QuantizationParameters")
        sv = vec(QP.QuantizationParametersStartScaleVector, [s], b.PrependFloat32)
        zv = vec(QP.QuantizationParametersStartZeroPointVector, [z], b.PrependInt64)
        QP.QuantizationParametersStart(b)
        QP.QuantizationParametersAddScale(b, sv)
        QP.QuantizationParametersAddZeroPoint(b, zv)
        q = QP.QuantizationParametersEnd(b)
        Tm = _mod("Tensor")
        Tm.TensorStart(b)
        Tm.TensorAddShape(b, sh)
        Tm.TensorAddType(b, _mod("TensorType").TensorType.INT8)
        Tm.TensorAddBuffer(b, 0)
        Tm.TensorAddName(b, nm)
        Tm.TensorAddQuantization(b, q)
        tens.append(Tm.TensorEnd(b))
    opts = None
    if opname == "LEAKY_RELU":
        L = _mod("LeakyReluOptions")
        L.LeakyReluOptionsStart(b)
        L.LeakyReluOptionsAddAlpha(b, alpha)
        opts = L.LeakyReluOptionsEnd(b)
    O = _mod("Operator")
    iv = vec(O.OperatorStartInputsVector, [0], b.PrependInt32)
    ov = vec(O.OperatorStartOutputsVector, [1], b.PrependInt32)
    O.OperatorStart(b)
    O.OperatorAddOpcodeIndex(b, 0)
    O.OperatorAddInputs(b, iv)
    O.OperatorAddOutputs(b, ov)
    if opts is not None:
        O.OperatorAddBuiltinOptionsType(b, _mod("BuiltinOptions").BuiltinOptions.LeakyReluOptions)
        O.OperatorAddBuiltinOptions(b, opts)
    opv = O.OperatorEnd(b)
    SG = _mod("SubGraph")
    tv = vec(SG.SubGraphStartTensorsVector, tens, b.PrependUOffsetTRelative)
    inv = vec(SG.SubGraphStartInputsVector, [0], b.PrependInt32)
    outv = vec(SG.SubGraphStartOutputsVector, [1], b.PrependInt32)
    ops = vec(SG.SubGraphStartOperatorsVector, [opv], b.PrependUOffsetTRelative)
    SG.SubGraphStart(b)
    SG.SubGraphAddTensors(b, tv)
    SG.SubGraphAddInputs(b, inv)
    SG.SubGraphAddOutputs(b, outv)
    SG.SubGraphAddOperators(b, ops)
    sg = SG.SubGraphEnd(b)
    code = getattr(_mod("BuiltinOperator").BuiltinOperator, opname)
    OC = _mod("OperatorCode")
    OC.OperatorCodeStart(b)
    OC.OperatorCodeAddDeprecatedBuiltinCode(b, min(code, 127))
    OC.OperatorCodeAddBuiltinCode(b, code)
    OC.OperatorCodeAddVersion(b, 1 if opname in ("HARD_SWISH", "RELU_N1_TO_1") else 2)
    oc = OC.OperatorCodeEnd(b)
    M = _mod("Model")
    ocv = vec(M.ModelStartOperatorCodesVector, [oc], b.PrependUOffsetTRelative)
    sgv = vec(M.ModelStartSubgraphsVector, [sg], b.PrependUOffsetTRelative)
    bv = vec(M.ModelStartBuffersVector, [buf0], b.PrependUOffsetTRelative)
    M.ModelStart(b)
    M.ModelAddVersion(b, 3)
    M.ModelAddOperatorCodes(b, ocv)
    M.ModelAddSubgraphs(b, sgv)
    M.ModelAddBuffers(b, bv)
    b.Finish(M.ModelEnd(b), file_identifier=b"TFL3")
    it = litert.Interpreter(model_content=bytes(b.Output()),
                            experimental_op_resolver_type=litert.OpResolverType.BUILTIN_REF)
    it.allocate_tensors()
    q = np.arange(-128, 128, dtype=np.int8).reshape(1, 256)
    it.set_tensor(it.get_input_details()[0]["index"], q)
    it.invoke()
    out = it.get_tensor(it.get_output_details()[0]["index"]).reshape(-1)
    tab = [0] * 256
    for v, r in zip(range(-128, 128), out):
        tab[v & 255] = int(r)
    return tab


def _strided_slice(op, T, v):
    x, begin, end, strides = (v[i] for i in op.inputs[:4])
    o = _options(op, "StridedSliceOptions")
    if o.EllipsisMask() or o.NewAxisMask() or o.Offset():
        raise _err(op, "ellipsis, new-axis and offset slices are not supported")
    idx = []
    for d in range(len(begin)):
        b = None if o.BeginMask() >> d & 1 else int(begin[d])
        e = None if o.EndMask() >> d & 1 else int(end[d])
        if o.ShrinkAxisMask() >> d & 1:
            idx.append(int(begin[d]))
        else:
            idx.append(slice(b, e, int(strides[d])))
    return np.asarray(x[tuple(idx)], np.int32)


def _pack(op, T, v):
    o = _options(op, "PackOptions")
    return np.stack([np.asarray(v[i]) for i in op.inputs], axis=o.Axis()).astype(np.int32)


def _concat(op, T, v):
    o = _options(op, "ConcatenationOptions")
    return np.concatenate([np.atleast_1d(v[i]) for i in op.inputs],
                          axis=o.Axis()).astype(np.int32)


# Shape arithmetic on int32 values, worked out here because every shape is
# known before the model runs.
FOLD = {"SHAPE": lambda op, T, v: np.asarray(T[op.inputs[0]].shape, np.int32),
        "STRIDED_SLICE": _strided_slice,
        "PACK": _pack,
        "CONCATENATION": _concat}


def fold(tensors, ops, outputs):
    """Works out the operators that only compute shapes, and returns the
    others with the values it found."""
    v = {t.idx: t.data for t in tensors if t.data is not None and t.dtype == "INT32"}
    rest = []
    for op in ops:
        ints = all(tensors[o].dtype == "INT32" and not tensors[o].scale
                   for o in op.outputs)
        needs = op.inputs[:1] if op.name == "SHAPE" else op.inputs
        if op.name in FOLD and ints and (op.name == "SHAPE" or all(i in v for i in needs)):
            if any(o in outputs for o in op.outputs):
                raise _err(op, "a model output that only holds a shape")
            v[op.outputs[0]] = FOLD[op.name](op, tensors, v)
        else:
            rest.append(op)
    return rest, v


def fold_pads(tensors, ops, outputs, v):
    """Finds each PAD that only feeds a VALID convolution, which can pad its
    input itself, and returns the other operators."""
    users = {}
    for op in ops:
        for i in op.inputs:
            users.setdefault(i, []).append(op)
    pads, rest = {}, []
    for op in ops:
        if op.name not in ("PAD", "PADV2"):
            rest.append(op)
            continue
        x, y = tensors[op.inputs[0]], tensors[op.outputs[0]]
        _int8(op, x, "input")
        _int8(op, y, "output")
        p = v.get(op.inputs[1])
        u = users.get(y.idx, [])
        ok = (p is not None and np.asarray(p).shape == (4, 2) and
              not np.asarray(p)[0].any() and not np.asarray(p)[3].any() and
              y.idx not in outputs and len(u) == 1 and
              u[0].name in ("CONV_2D", "DEPTHWISE_CONV_2D") and u[0].inputs[0] == y.idx)
        if ok and op.name == "PADV2":
            c = tensors[op.inputs[2]]
            ok = c.data is not None and int(c.data.reshape(-1)[0]) == x.zp[0]
        if not ok:
            raise _err(op, "only a PAD that feeds one convolution is supported")
        _same_quant(op, x, y)
        p = np.asarray(p)
        pads[y.idx] = (x.idx, int(p[1][0]), int(p[2][0]))
    return rest, pads


LOWER = {"FULLY_CONNECTED": lower_fully_connected,
         "CONV_2D": lower_conv,
         "DEPTHWISE_CONV_2D": lower_depthwise,
         "MAX_POOL_2D": lower_max_pool,
         "AVERAGE_POOL_2D": lower_average_pool,
         "MEAN": lower_mean,
         "ADD": lower_add,
         "CONCATENATION": lower_concatenation,
         "SOFTMAX": lower_softmax,
         "RESHAPE": lower_reshape,
         "LEAKY_RELU": lower_unary}
LOWER.update({k: lower_unary for k in UNARY})


def lower(tensors, ops, ctx):
    layers = []
    for op in ops:
        if op.name not in LOWER:
            raise ConvertError(f"op {op.idx} ({op.name}) is not supported; flexnn "
                               f"converts {', '.join(sorted(LOWER | {'PAD': 0}))}")
        layers.append(LOWER[op.name](op, tensors, ctx))
    return layers


def fusible(layers, outputs):
    """Each depthwise layer that fnn_dwpw_s8() can run with the 1 x 1
    convolution reading its output, mapped to that convolution."""
    find = _alias_root(layers)
    ends = {find(t) for t in outputs}
    pairs = {}
    for i, L in enumerate(layers):
        if L.kind != "depthwise":
            continue
        r = find(L.writes[0])
        readers = [j for j, M in enumerate(layers)
                   if M.kind != "alias" and any(find(t) == r for t in M.reads)]
        if r in ends or len(readers) != 1:
            continue
        P = layers[readers[0]]
        h, w, _, oh, ow, _, _, _, sh, sw, _, _ = L.geom
        if (P.kind == "conv" and P.reads == L.writes and sh == sw == 1 and
                (oh, ow) == (h, w) and P.geom[6:10] == (1, 1, 1, 1)):
            pairs[i] = readers[0]
    return pairs


def fuse(layers, pairs):
    """The layers with each pair in pairs run as one fnn_dwpw_s8() call."""
    by_pw = {j: i for i, j in pairs.items()}
    out = []
    for j, P in enumerate(layers):
        if j in pairs:
            continue
        if j not in by_pw:
            out.append(P)
            continue
        D = layers[by_pw[j]]
        _, w, c, _, _, c_dw, kh = D.geom[:7]
        F = Layer(D.op, "dwpw", D.reads, P.writes, D.decls + P.decls,
                  f"fnn_dwpw_s8(&l{D.op.idx}, &l{P.op.idx}, {{x0}}, {{y}}, {{s}});",
                  D.flash + P.flash, D.macs + P.macs, scratch=kh * w * c + w * c_dw,
                  in_place=(0,) if P.geom[5] <= c else (), packed=P.packed)
        F.ops = (D.op, P.op)
        out.append(F)
    return out


def _alias_root(layers):
    group = {}

    def find(t):
        while group.get(t, t) != t:
            t = group[t]
        return t
    for L in layers:
        if L.kind == "alias":
            group[find(L.writes[0])] = find(L.reads[0])
    return find


class Plan:
    def __init__(self, where, scratch, arena, bound, sliced, over):
        self.where, self.scratch, self.arena, self.bound = where, scratch, arena, bound
        self.sliced, self.over = sliced, over


def plan(tensors, layers, inputs, outputs, in_place=True, slices=True):
    """Live buffers share bytes only for an output over a dying input or a join's slices."""
    find = _alias_root(layers)
    ext = {}
    for t in inputs:
        ext[find(t)] = "in"
    for t in outputs:
        if ext.get(find(t)) == "in":
            raise ConvertError("an output is the input itself")
        ext[find(t)] = "out"
    run = [i for i, L in enumerate(layers) if L.kind != "alias"]
    life, size = {}, {}
    for i in run:
        L = layers[i]
        for t in L.reads + L.writes:
            r = find(t)
            if r not in ext:
                a, b = life.get(r, (i, i))
                life[r] = (min(a, i), max(b, i))
                size[r] = tensors[r].size()
        if L.scratch:
            life[("s", i)] = (i, i)
            size[("s", i)] = L.scratch

    # Buffers tied together sit at fixed offsets from each other.
    rel = {b: (b, 0) for b in life}
    members = {b: [b] for b in life}
    parent, handoff, sliced, over = {}, {}, set(), {}

    def join(a, b, off):
        (ra, oa), (rb, ob) = rel[a], rel[b]
        for m in members.pop(ra):
            rel[m] = (rb, rel[m][1] + ob + off - oa)
            members[rb].append(m)

    def holds(a, b):
        while b in parent:
            b = parent[b]
            if b == a:
                return True
        return False

    def consistent(root):
        ms = members[root]
        for j, p in enumerate(ms):
            for q in ms[j + 1:]:
                (op, oq), (lp, lq) = (rel[p][1], rel[q][1]), (life[p], life[q])
                if op >= oq + size[q] or oq >= op + size[p]:
                    continue
                if lp[1] < lq[0] or lq[1] < lp[0] or holds(p, q) or holds(q, p):
                    continue
                i = handoff.get((p, q), handoff.get((q, p)))
                if i is None or op != oq or max(lp[0], lq[0]) != i or min(lp[1], lq[1]) != i:
                    return False
        return True

    def attempt(tie):
        saved = (dict(rel), {r: list(m) for r, m in members.items()}, dict(parent),
                 dict(handoff))
        root = tie()
        if consistent(root):
            return True
        rel.clear(), members.clear(), parent.clear(), handoff.clear()
        rel.update(saved[0]), members.update(saved[1]), parent.update(saved[2])
        handoff.update(saved[3])
        return False

    for i in run if slices else ():
        L = layers[i]
        c, xs = find(L.writes[0]), [find(t) for t in L.reads]
        if (L.kind != "concat" or L.outer != 1 or c in ext or c in xs or
                len(set(xs)) != len(xs) or any(x in ext or x in parent for x in xs) or
                any(rel[x][0] != x for x in xs)):
            continue

        def tie(c=c, xs=xs, L=L):
            at = 0
            for x, t in zip(xs, L.reads):
                join(x, c, at)
                parent[x] = c
                at += tensors[t].size()
            return rel[c][0]
        if attempt(tie):
            sliced.add(i)

    for i in run if in_place else ():
        L = layers[i]
        y = find(L.writes[0])
        for k in L.in_place:
            x = find(L.reads[k])
            if y in ext or x in ext or x == y or life[x][1] != i or rel[x][0] == rel[y][0]:
                continue

            def tie(x=x, y=y, i=i):
                handoff[(x, y)] = i
                join(x, y, 0)
                return rel[y][0]
            if attempt(tie):
                over[i] = k
                break

    for r, ms in members.items():
        low = min(rel[m][1] for m in ms)
        for m in ms:
            rel[m] = (r, rel[m][1] - low)

    def extent(r):
        return max(rel[m][1] + size[m] for m in members[r])
    offset, placed = {}, []
    for r in sorted(members, key=lambda r: (-extent(r),
                                            min(life[m][0] for m in members[r]))):
        ms = [(m, rel[m][1]) for m in members[r]]
        bases = sorted({0} | {(e - o + 3) & ~3 for _, e, _, _ in placed for _, o in ms
                              if e >= o})
        for base in bases:
            if all(s >= base + o + size[m] or base + o >= e or b < life[m][0] or
                   life[m][1] < a for m, o in ms for s, e, a, b in placed):
                break
        for m, o in ms:
            offset[m] = base + o
            placed.append((base + o, base + o + size[m]) + life[m])
    arena = max((e for _, e, _, _ in placed), default=0)
    bound = 0
    for i in run:
        live = 0
        for ms in members.values():
            spans = sorted((rel[m][1], rel[m][1] + size[m]) for m in ms
                           if life[m][0] <= i <= life[m][1])
            end = 0
            for s, e in spans:
                live += max(e, end) - max(s, end)
                end = max(end, e)
        bound = max(bound, live)
    where = {t: ext.get(find(t), offset.get(find(t))) for t in range(len(tensors))}
    scratch = {i: offset[("s", i)] for i in run if ("s", i) in offset}
    return Plan(where, scratch, (arena + 3) & ~3, bound, sliced, over)


def verify(tensors, layers, inputs, outputs, p):
    """Replays the plan byte by byte, each byte tagged with the value it holds."""
    find = _alias_root(layers)
    made = {find(layers[i].writes[0]): layers[i] for i in p.sliced}

    def tags(t):
        r = find(t)
        if r in made:
            L = made[r]
            rows = max(L.outer, 1)
            return np.hstack([tags(x).reshape(rows, -1) for x in L.reads]).reshape(-1)
        return r * (1 << 24) + np.arange(tensors[r].size(), dtype=np.int64)
    mem = {"arena": np.full(p.arena, -1, np.int64),
           "out": np.full(tensors[outputs[0]].size(), -1, np.int64)}

    def span(t):
        w = p.where[t]
        return ("arena", w) if isinstance(w, int) else (w, 0)

    def fail(i, why):
        raise ConvertError(f"the memory plan is wrong at op {layers[i].op.idx}: {why}")
    for i, L in enumerate(layers):
        if L.kind == "alias" or i in p.sliced:
            continue
        ys, yo = span(L.writes[0])
        yn = tensors[L.writes[0]].size()
        if ys == "in":
            fail(i, "it writes the model's input")
        for k, t in enumerate(L.reads):
            s, o = span(t)
            if s == "in":
                if find(t) != find(inputs[0]):
                    fail(i, f"it reads '{tensors[t].name}' from the input")
                continue
            n = tensors[t].size()
            if o + n > len(mem[s]) or not np.array_equal(mem[s][o:o + n], tags(t)):
                fail(i, f"'{tensors[t].name}' was overwritten before it was read")
            if s == ys and o < yo + yn and yo < o + n and not (k in L.in_place and o == yo):
                fail(i, f"the output overlaps '{tensors[t].name}'")
        if any(isinstance(w, int) and w < 0 for w in
               [p.where[t] for t in L.reads + L.writes] + [p.scratch.get(i, 0)]):
            fail(i, "a buffer starts before the arena")
        if L.scratch:
            so = p.scratch[i]
            busy = [span(t) + (tensors[t].size(),) for t in L.reads + L.writes]
            if so + L.scratch > p.arena or any(s == "arena" and so < o + n and o < so + L.scratch
                                               for s, o, n in busy):
                fail(i, "the scratch overlaps a tensor")
            mem["arena"][so:so + L.scratch] = -2
        if yo + yn > len(mem[ys]):
            fail(i, "the output runs past its buffer")
        mem[ys][yo:yo + yn] = tags(L.writes[0])
    t = outputs[0]
    s, o = span(t)
    if s != "out" or not np.array_equal(mem["out"], tags(t)):
        raise ConvertError("the memory plan leaves the wrong values in the output")


def _array(ctype, name, values, per_line=16, align=False):
    vals = [str(v) if ctype != "int32_t" or v != -(1 << 31) else "(-2147483647 - 1)"
            for v in values]
    if ctype == "uint32_t":
        vals = [v + "u" for v in vals]
    lines = [", ".join(vals[i:i + per_line]) for i in range(0, len(vals), per_line)]
    body = ",\n    ".join(lines)
    spec = "static _Alignas(4) const" if align else "static const"
    return f"{spec} {ctype} {name}[{len(values)}] = {{\n    {body}}};\n"


def _float(v):
    """A float32 as the shortest C float literal that reads back the same."""
    s = str(np.float32(v))
    return s + ("f" if ("." in s or "e" in s) else ".0f")


def ident(s):
    s = re.sub(r"[^0-9a-zA-Z_]", "_", s).lower()
    return "m_" + s if s[:1].isdigit() else s


def emit(name, src, tensors, layers, p, inputs, outputs):
    up = name.upper()
    tin, tout = tensors[inputs[0]], tensors[outputs[0]]
    banner = f"/* generated by fnn_tflite from {src}: do not edit */\n"
    h = [banner,
         f"#ifndef {up}_H\n#define {up}_H\n\n#include <stdint.h>\n\n",
         f"#define {up}_IN_LEN {tin.size()}u\n",
         f"#define {up}_OUT_LEN {tout.size()}u\n",
         f"/* Bytes of working memory the model needs, 4-byte aligned. */\n"
         f"#define {up}_ARENA_SIZE {max(p.arena, 4)}u\n",
         f"#define {up}_IN_SCALE {_float(tin.scale[0])}\n",
         f"#define {up}_IN_ZERO_POINT ({tin.zp[0]})\n",
         f"#define {up}_OUT_SCALE {_float(tout.scale[0])}\n",
         f"#define {up}_OUT_ZERO_POINT ({tout.zp[0]})\n\n",
         f"/* Runs the model on the {up}_IN_LEN values at in and writes {up}_OUT_LEN\n"
         f" * values to out, using the {up}_ARENA_SIZE bytes at arena. */\n",
         f"void {name}_run(const int8_t *in, int8_t *out, uint8_t *arena);\n\n",
         f"#endif /* {up}_H */\n"]
    c = [banner, f'#include "{name}.h"\n#include "fnn.h"\n\n']
    if any(L.packed for L in layers):
        c.append(f"_Static_assert(FNN_LAYOUT_VERSION == {LAYOUT_VERSION},\n"
                 '               "these weights are packed for another version of flexnn: "\n'
                 '               "convert the model again");\n\n')
    run = []

    def buf(t, const):
        w = p.where[t]
        if w in ("in", "out"):
            return w
        return f"({'const ' if const else ''}int8_t *)(arena + {w})"
    for i, L in enumerate(layers):
        if L.kind == "alias" or i in p.sliced:
            continue
        c.append(L.decls + "\n")
        bufs = {f"x{k}": buf(t, True) for k, t in enumerate(L.reads)}
        bufs["y"] = buf(L.writes[0], False)
        if L.scratch:
            bufs["s"] = f"(int8_t *)(arena + {p.scratch[i]})"
        run.append("  " + L.call.format(**bufs) + "\n")
    c.append(f"void {name}_run(const int8_t *in, int8_t *out, uint8_t *arena) {{\n")
    if not any("arena" in r for r in run):
        c.append("  (void)arena;\n")
    c.extend(run)
    c.append("}\n")
    return "".join(h), "".join(c)


def convert(path, out_dir, name=None, tflite_tables=False):
    """Converts path into out_dir/<name>.h and .c; returns the name and a
    report."""
    tensors, ops, inputs, outputs = load(path)
    if len(inputs) != 1 or len(outputs) != 1:
        raise ConvertError("flexnn converts models with one input and one output")
    for t, what in ((tensors[inputs[0]], "input"), (tensors[outputs[0]], "output")):
        if t.dtype != "INT8":
            raise ConvertError(f"the model's {what} '{t.name}' is {t.dtype}; "
                               "convert with int8 inputs and outputs")
    rest, v = fold(tensors, ops, outputs)
    rest, pads = fold_pads(tensors, rest, outputs, v)
    ctx = {"T": tensors, "v": v, "pad": pads, "tflite_tables": tflite_tables}
    lowered = lower(tensors, rest, ctx)

    def best(layers):
        return min((plan(tensors, layers, inputs, outputs, a, b) for a in (True, False)
                    for b in (True, False)),
                   key=lambda q: (q.arena, -len(q.sliced), -len(q.over)))
    # A fused pair runs slower and is kept only when it shrinks the arena.
    pairs = can = fusible(lowered, outputs)
    p = best(fuse(lowered, pairs))
    for i in list(pairs):
        fewer = {k: j for k, j in pairs.items() if k != i}
        q = best(fuse(lowered, fewer))
        if q.arena <= p.arena:
            pairs, p = fewer, q
    layers = fuse(lowered, pairs)
    verify(tensors, layers, inputs, outputs, p)
    name = ident(name or Path(path).stem)
    h, c = emit(name, Path(path).name, tensors, layers, p, inputs, outputs)
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{name}.h").write_text(h)
    (out / f"{name}.c").write_text(c)
    folded = len(ops) - len(lowered) - len(pads)
    idx = ["+".join(str(o.idx) for o in L.ops) for L in layers]
    wide = max(3, *map(len, idx))
    rows = [f"{name}: {len(ops)} operators, {folded} of them only shapes and "
            f"{len(pads)} padding{'' if len(pads) == 1 else 's'}. Arena {p.arena} B, "
            f"with {p.bound} B live at the "
            f"busiest layer. Constants {sum(L.flash for L in layers)} B." +
            (f" {len(pairs)} of {len(can)} depthwise and pointwise pairs fused." if can
             else ""),
            f"  {'op':>{wide}}  {'operator':18} {'kernel':9} {'MACs':>9} {'flash':>7}  "
            f"{'reads':12} {'writes':16} scratch"]
    for i, L in enumerate(layers):
        if L.kind == "alias":
            continue
        reads = ", ".join(str(p.where[t]) for t in L.reads)
        writes = str(p.where[L.writes[0]]) + (", in place" if i in p.over else "")
        kind = "no copy" if i in p.sliced else L.kind
        rows.append(f"  {idx[i]:>{wide}}  {L.op.name:18} {kind:9} {L.macs:9} {L.flash:5} B  "
                    f"{reads:12} {writes:16} {p.scratch.get(i, '')}".rstrip())
    return name, "\n".join(rows)


ROUNDING = {"up": 0, "away": 1, "gemmlowp": 2, "tflite": 3}


def build(c_dir, name, rounding, lib_dir):
    """Compiles the generated C with flexnn into a shared library."""
    srcs = [str(Path(c_dir) / f"{name}.c")]
    for d in ("nn", "math", "utils"):
        srcs += sorted(str(p) for p in (ROOT / d / "src").glob("*.c"))
    inc = [f"-I{ROOT / d}" for d in ("arch", "nn/include", "math/include",
                                     "utils/include")]
    lib = Path(lib_dir) / f"lib{name}_{rounding}.so"
    cmd = (["cc", "-std=c11", "-O2", "-fPIC", "-shared", "-Wall", "-Wextra",
            "-Wshadow", "-Wconversion", "-Wsign-conversion", "-Werror",
            f"-DFNN_ROUNDING={ROUNDING[rounding]}"] + inc + srcs + ["-lm", "-o", str(lib)])
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise ConvertError("the generated C does not build:\n" + r.stderr)
    return lib


def check(path, c_dir, name, rounding="tflite", runs=200, seed=0, feeds=None):
    """Runs the model in the TFLite interpreter's reference kernels and in the
    generated C; returns (outputs compared, outputs that differ)."""
    import ctypes
    from ai_edge_litert import interpreter as litert
    it = litert.Interpreter(model_path=str(path),
                            experimental_op_resolver_type=litert.OpResolverType.BUILTIN_REF)
    it.allocate_tensors()
    di, do = it.get_input_details()[0], it.get_output_details()[0]
    if feeds is None:
        rng = np.random.default_rng(seed)
        feeds = [rng.integers(-128, 128, di["shape"], dtype=np.int8) for _ in range(runs)]
    with tempfile.TemporaryDirectory() as tmp:
        lib = ctypes.CDLL(str(build(c_dir, name, rounding, tmp)))
        fn = getattr(lib, f"{name}_run")
        fn.argtypes = [ctypes.c_void_p] * 3
        n_out = int(np.prod(do["shape"]))
        arena = np.zeros(1 << 20, np.uint32)
        bad = 0
        for x in feeds:
            it.set_tensor(di["index"], x)
            it.invoke()
            want = it.get_tensor(do["index"]).reshape(-1)
            xc = np.ascontiguousarray(x.reshape(-1))
            got = np.zeros(n_out, np.int8)
            fn(xc.ctypes.data, got.ctypes.data, arena.ctypes.data)
            bad += int(np.count_nonzero(got != want))
    return len(feeds) * n_out, bad


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("model", help="a full-integer int8 .tflite model")
    ap.add_argument("-o", "--out", default=".", help="where to write <name>.h and .c")
    ap.add_argument("--name", help="the C prefix; the file name by default")
    ap.add_argument("--check", action="store_true",
                    help="compare the C with the TFLite interpreter")
    ap.add_argument("--rounding", choices=ROUNDING, default="tflite",
                    help="FNN_ROUNDING for --check (tflite: the interpreter's "
                         "rule for each operator)")
    ap.add_argument("--inputs", help="a .npy file of inputs for --check")
    ap.add_argument("--tflite-tables", action="store_true",
                    help="read activation tables from the TFLite interpreter "
                         "instead of computing them exactly")
    a = ap.parse_args(argv)
    try:
        name, report = convert(a.model, a.out, a.name, a.tflite_tables)
        print(report)
        if a.check:
            feeds = None
            if a.inputs:
                arr = np.load(a.inputs).astype(np.int8)
                feeds = list(arr)
            n, bad = check(a.model, a.out, name, a.rounding, feeds=feeds)
            print(f"check ({a.rounding}): {bad} of {n} outputs differ from the "
                  "TFLite interpreter")
            return 1 if bad else 0
    except (ConvertError, OSError) as e:
        print(f"fnn_tflite: {e}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
