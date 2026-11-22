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


def act_range(act, scale, zp):
    """TFLite's CalculateActivationRangeQuantized for int8, in float32."""
    def quant(f):
        v = float(np.float32(f) / np.float32(scale))
        return zp + int(math.floor(abs(v) + 0.5)) * (1 if v >= 0 else -1)
    lo, hi = -128, 127
    if act == 1:                                    # RELU
        lo = max(lo, quant(0.0))
    elif act == 2:                                  # RELU_N1_TO_1
        lo, hi = max(lo, quant(-1.0)), min(hi, quant(1.0))
    elif act == 3:                                  # RELU6
        lo, hi = max(lo, quant(0.0)), min(hi, quant(6.0))
    elif act != 0:
        raise ConvertError(f"fused activation {act} is not supported")
    return lo, hi


def wrap32(v):
    return int((int(v) + (1 << 31)) % (1 << 32) - (1 << 31))



class Layer:
    """One flexnn call: its kind, the tensors it reads and writes, and what the
    emitter needs to write it out."""

    def __init__(self, op, kind, reads, writes, **f):
        self.op, self.kind, self.reads, self.writes, self.f = op, kind, reads, writes, f


def _int8(op, t, what):
    if t.dtype != "INT8":
        raise ConvertError(f"op {op.idx} ({op.name}): {what} '{t.name}' is "
                           f"{t.dtype}; flexnn takes int8")
    if len(t.scale) < 1 or len(t.zp) < 1:
        raise ConvertError(f"op {op.idx} ({op.name}): {what} '{t.name}' is not "
                           "quantised")


def lower_fully_connected(op, T):
    x, w, y = T[op.inputs[0]], T[op.inputs[1]], T[op.outputs[0]]
    b = T[op.inputs[2]] if len(op.inputs) > 2 and op.inputs[2] >= 0 else None
    for t, what in ((x, "input"), (w, "weights"), (y, "output")):
        _int8(op, t, what)
    o = _options(op, "FullyConnectedOptions")
    if o.WeightsFormat() != 0:
        raise ConvertError(f"op {op.idx} ({op.name}): shuffled weights are not "
                           "supported")
    if w.data is None or len(w.shape) != 2:
        raise ConvertError(f"op {op.idx} ({op.name}): the weights must be a "
                           "constant [outputs][inputs] tensor")
    n_out, n_in = w.shape
    if x.size() != n_in:
        raise ConvertError(f"op {op.idx} ({op.name}): {x.size()} inputs for "
                           f"weights of {n_in}; batches are not supported")
    if any(z != 0 for z in w.zp):
        raise ConvertError(f"op {op.idx} ({op.name}): weights must have zero "
                           "points of 0")
    bias = np.zeros(n_out, np.int64) if b is None else b.data.astype(np.int64)
    if b is not None and b.dtype != "INT32":
        raise ConvertError(f"op {op.idx} ({op.name}): the bias is {b.dtype}, "
                           "not int32")
    zx, zy = x.zp[0], y.zp[0]
    wsum = w.data.astype(np.int64).sum(axis=1)
    folded = [wrap32(bias[c] - zx * wsum[c]) for c in range(n_out)]
    f32 = np.float32
    s_w = w.scale if len(w.scale) == n_out else w.scale[:1]
    eff = [float(f32(x.scale[0]) * f32(s)) / float(f32(y.scale[0])) for s in s_w]
    q = [quantize_multiplier(e) for e in eff]
    lo, hi = act_range(o.FusedActivationFunction(), y.scale[0], zy)
    return Layer(op, "dense", [x.idx], [y.idx], w=w.data, b=folded, n_in=n_in,
                 n_out=n_out, mult=[m for m, _ in q], rsh=[r for _, r in q],
                 zp=zy, lo=lo, hi=hi, macs=n_in * n_out)


def lower_softmax(op, T):
    x, y = T[op.inputs[0]], T[op.outputs[0]]
    _int8(op, x, "input")
    _int8(op, y, "output")
    if y.zp[0] != -128 or y.scale[0] != float(np.float32(1.0 / 256.0)):
        raise ConvertError(f"op {op.idx} ({op.name}): the output must have "
                           "TFLite's scale of 1/256 and zero point of -128")
    beta = _options(op, "SoftmaxOptions").Beta()
    n = x.shape[-1] if x.shape else 1
    if x.size() != n:
        raise ConvertError(f"op {op.idx} ({op.name}): softmax over more than "
                           "one row is not supported")
    k = float(beta) * x.scale[0]
    if not (k > 0.0) or not math.isfinite(k):
        raise ConvertError(f"op {op.idx} ({op.name}): beta times the input "
                           "scale must be positive")
    return Layer(op, "softmax", [x.idx], [y.idx], tab=lower_softmax_table(k),
                 n=n, macs=0)


def lower_softmax_table(k):
    """The exponentials fnn_softmax_s8_build() writes, for beta times the
    input scale k."""
    return [int(math.ldexp(math.exp(-k * d), 31) + 0.5) for d in range(256)]


def lower_reshape(op, T):
    x, y = T[op.inputs[0]], T[op.outputs[0]]
    if x.size() != y.size() or x.dtype != y.dtype:
        raise ConvertError(f"op {op.idx} ({op.name}): input and output differ "
                           "in size")
    return Layer(op, "alias", [x.idx], [y.idx], macs=0)


def _strided_slice(op, T, v):
    x, begin, end, strides = (v[i] for i in op.inputs[:4])
    o = _options(op, "StridedSliceOptions")
    if o.EllipsisMask() or o.NewAxisMask() or o.Offset():
        raise ConvertError(f"op {op.idx} ({op.name}): ellipsis, new-axis and "
                           "offset slices are not supported")
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
    others."""
    v = {t.idx: t.data for t in tensors if t.data is not None and t.dtype == "INT32"}
    rest = []
    for op in ops:
        ints = all(tensors[o].dtype == "INT32" and not tensors[o].scale
                   for o in op.outputs)
        needs = op.inputs[:1] if op.name == "SHAPE" else op.inputs
        if op.name in FOLD and ints and (op.name == "SHAPE" or all(i in v for i in needs)):
            if any(o in outputs for o in op.outputs):
                raise ConvertError(f"op {op.idx} ({op.name}): a model output "
                                   "that only holds a shape")
            v[op.outputs[0]] = FOLD[op.name](op, tensors, v)
        else:
            rest.append(op)
    return rest


LOWER = {"FULLY_CONNECTED": lower_fully_connected,
         "SOFTMAX": lower_softmax,
         "RESHAPE": lower_reshape}


def lower(tensors, ops):
    layers = []
    for op in ops:
        if op.name not in LOWER:
            raise ConvertError(f"op {op.idx} ({op.name}) is not supported; "
                               f"flexnn converts {', '.join(sorted(LOWER))}")
        layers.append(LOWER[op.name](op, tensors))
    return layers



def plan(tensors, layers, inputs, outputs):
    """Gives every tensor a buffer: the caller's input or output, or an
    arena offset. Tensors whose lifetimes overlap never share bytes."""
    group = {}

    def find(t):
        while group.get(t, t) != t:
            t = group[t]
        return t
    for L in layers:
        if L.kind == "alias":
            group[find(L.writes[0])] = find(L.reads[0])
    ext = {}
    for t in inputs:
        ext[find(t)] = "in"
    for t in outputs:
        if ext.get(find(t)) == "in":
            raise ConvertError("an output is the input itself")
        ext[find(t)] = "out"
    life = {}
    for i, L in enumerate(layers):
        if L.kind == "alias":
            continue
        for t in L.reads + L.writes:
            r = find(t)
            if r not in ext:
                a, b = life.get(r, (i, i))
                life[r] = (min(a, i), max(b, i))
    size = {r: tensors[r].size() for r in life}
    offset = {}
    for r in sorted(life, key=lambda r: (-size[r], life[r][0])):
        a, b = life[r]
        busy = sorted((offset[o], offset[o] + size[o]) for o in offset
                      if life[o][0] <= b and a <= life[o][1])
        at = 0
        for lo, hi in busy:
            if at + size[r] <= lo:
                break
            at = max(at, (hi + 3) & ~3)
        offset[r] = at
    arena = max((offset[r] + size[r] for r in offset), default=0)
    peak = max((sum(size[r] for r in life if life[r][0] <= i <= life[r][1])
                for i in range(len(layers))), default=0)
    where = {}
    for t in range(len(tensors)):
        r = find(t)
        where[t] = ext.get(r, offset.get(r))
    return where, (arena + 3) & ~3, peak



def _array(ctype, name, values, per_line=16, align=False):
    vals = [str(v) if ctype != "int32_t" or v != -(1 << 31) else "(-2147483647 - 1)"
            for v in values]
    if ctype == "uint32_t":
        vals = [v + "u" for v in vals]
    lines = [", ".join(vals[i:i + per_line]) for i in range(0, len(vals), per_line)]
    body = ",\n    ".join(lines)
    al = " FX_ALIGN(4)" if align else ""
    return f"static const {ctype} {name}[{len(values)}]{al} = {{\n    {body}}};\n"


def _float(v):
    """A float32 as the shortest C float literal that reads back the same."""
    s = str(np.float32(v))
    return s + ("f" if ("." in s or "e" in s) else ".0f")


def ident(s):
    s = re.sub(r"[^0-9a-zA-Z_]", "_", s).lower()
    return "m_" + s if s[:1].isdigit() else s


def emit(name, src, tensors, layers, where, arena, inputs, outputs):
    up = name.upper()
    tin, tout = tensors[inputs[0]], tensors[outputs[0]]
    banner = (f"/* generated by fnn_tflite from {src}: do not edit */\n")
    h = [banner,
         f"#ifndef {up}_H\n#define {up}_H\n\n#include <stdint.h>\n\n",
         f"#define {up}_IN_LEN {tin.size()}u\n",
         f"#define {up}_OUT_LEN {tout.size()}u\n",
         f"/* bytes, 4-byte aligned */\n#define {up}_ARENA_SIZE {max(arena, 4)}u\n",
         f"#define {up}_IN_SCALE {_float(tin.scale[0])}\n",
         f"#define {up}_IN_ZERO_POINT ({tin.zp[0]})\n",
         f"#define {up}_OUT_SCALE {_float(tout.scale[0])}\n",
         f"#define {up}_OUT_ZERO_POINT ({tout.zp[0]})\n\n",
         f"/* Runs the model on {up}_IN_LEN values into {up}_OUT_LEN; arena holds\n"
         f" * {up}_ARENA_SIZE bytes, 4-byte aligned. */\n",
         f"void {name}_run(const int8_t *in, int8_t *out, uint8_t *arena);\n\n",
         f"#endif /* {up}_H */\n"]
    c = [banner, f'#include "{name}.h"\n#include "fnn.h"\n\n']
    run = []
    used = set()

    def buf(t, const):
        w = where[t]
        if w == "in":
            return "in"
        if w == "out":
            return "out"
        used.add(w)
        return f"(int8_t *)(arena + {w})" if not const else f"(const int8_t *)(arena + {w})"
    for i, L in enumerate(layers):
        if L.kind == "alias":
            continue
        f = L.f
        x, y = buf(L.reads[0], True), buf(L.writes[0], False)
        if L.kind == "dense":
            c.append(_array("int8_t", f"w{i}", f["w"].reshape(-1).tolist()))
            c.append(_array("int32_t", f"b{i}", f["b"], per_line=8))
            c.append(_array("int32_t", f"m{i}", f["mult"], per_line=8))
            c.append(_array("int8_t", f"r{i}", f["rsh"]))
            c.append(f"static const FNN_Dense l{i} = {{w{i}, b{i}, {f['n_in']}u, "
                     f"{f['n_out']}u, 0u, {{m{i}, r{i}, {len(f['mult'])}u}}, "
                     f"{{{f['zp']}, {f['lo']}, {f['hi']}}}}};\n\n")
            run.append(f"  fnn_dense_s8(&l{i}, {x}, {y});\n")
        elif L.kind == "softmax":
            c.append(_array("uint32_t", f"e{i}", f["tab"], per_line=6))
            c.append("\n")
            run.append(f"  fnn_softmax_s8({x}, {y}, {f['n']}u, e{i});\n")
    c.append(f"void {name}_run(const int8_t *in, int8_t *out, uint8_t *arena) {{\n")
    if not used:
        c.append("  (void)arena;\n")
    c.extend(run)
    c.append("}\n")
    return "".join(h), "".join(c)


def convert(path, out_dir, name=None):
    """Converts path into out_dir/<name>.h and .c; returns the name and a
    report."""
    tensors, ops, inputs, outputs = load(path)
    if len(inputs) != 1 or len(outputs) != 1:
        raise ConvertError("flexnn converts models with one input and one output")
    for t, what in ((tensors[inputs[0]], "input"), (tensors[outputs[0]], "output")):
        if t.dtype != "INT8":
            raise ConvertError(f"the model's {what} '{t.name}' is {t.dtype}; "
                               "convert with int8 inputs and outputs")
    layers = lower(tensors, fold(tensors, ops, outputs))
    where, arena, peak = plan(tensors, layers, inputs, outputs)
    name = ident(name or Path(path).stem)
    h, c = emit(name, Path(path).name, tensors, layers, where, arena, inputs, outputs)
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{name}.h").write_text(h)
    (out / f"{name}.c").write_text(c)
    rows = []
    flash = 0
    for i, L in enumerate(layers):
        if L.kind == "alias":
            continue
        n = 0
        if L.kind == "dense":
            n = L.f["w"].size + 4 * (len(L.f["b"]) + len(L.f["mult"])) + len(L.f["rsh"])
        elif L.kind == "softmax":
            n = 4 * 256
        flash += n
        rows.append(f"  {L.op.idx:3}  {L.op.name:18} {L.kind:8} {L.f.get('macs', 0):9} MACs"
                    f"  {n:7} B   -> {where[L.writes[0]]}")
    report = (f"{name}: {len(ops)} operators, {len(ops) - len(layers)} of them only "
              f"shapes; arena {arena} B (live tensors peak at {peak} B), constants "
              f"{flash} B\n" + "\n".join(rows))
    return name, report



ROUNDING = {"up": 0, "away": 1, "gemmlowp": 2}


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


def check(path, c_dir, name, rounding="away", runs=200, seed=0, feeds=None):
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
    ap.add_argument("--rounding", choices=ROUNDING, default="away",
                    help="FNN_ROUNDING for --check (away: the interpreter's "
                         "fully connected rule)")
    ap.add_argument("--inputs", help="a .npy file of inputs for --check")
    a = ap.parse_args(argv)
    try:
        name, report = convert(a.model, a.out, a.name)
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
