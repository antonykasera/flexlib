"""Writes small TFLite models straight from the flatbuffer schema, without
TensorFlow. To support another operator, add it to OPTIONS."""

import importlib

import flatbuffers
import numpy as np


def _mod(name):
    # `import tflite.Buffer` gives the class, which hides the module
    return importlib.import_module("tflite." + name)


_B = {n: _mod(n) for n in ("Model", "SubGraph", "Tensor", "Operator",
                           "OperatorCode", "Buffer", "QuantizationParameters")}
BuiltinOperator = _mod("BuiltinOperator").BuiltinOperator
BuiltinOptions = _mod("BuiltinOptions").BuiltinOptions
TensorType = _mod("TensorType").TensorType
INT8, INT32 = TensorType.INT8, TensorType.INT32

# for each operator: its options table, BuiltinOptions member, version, defaults
OPTIONS = {
    "FULLY_CONNECTED": ("FullyConnectedOptions", "FullyConnectedOptions", 4,
                        {"FusedActivationFunction": 0}),
    "LOGISTIC": (None, None, 2, {}),
    "TANH": (None, None, 2, {}),
    "HARD_SWISH": (None, None, 1, {}),
    "LEAKY_RELU": ("LeakyReluOptions", "LeakyReluOptions", 2, {"Alpha": 0.2}),
    "SOFTMAX": ("SoftmaxOptions", "SoftmaxOptions", 2, {"Beta": 1.0}),
    "ARG_MAX": ("ArgMaxOptions", "ArgMaxOptions", 2,
                {"OutputType": TensorType.INT32}),
}

_NP = {INT8: np.int8, INT32: np.int32}


def _vector(b, start, items, prepend):
    start(b, len(items))
    for it in reversed(items):
        prepend(it)
    return b.EndVector()


class Model:
    def __init__(self):
        self.tensors = []
        self.ops = []
        self.inputs = []
        self.outputs = []

    def tensor(self, name, shape, ttype, scale=None, zp=None, axis=0, data=None):
        """Adds a tensor and returns its index. With data it is a constant;
        a list of scales makes it per-channel along axis."""
        scales = None if scale is None else list(np.atleast_1d(scale))
        zps = None if zp is None else list(np.atleast_1d(zp))
        if data is not None:
            data = np.asarray(data, _NP[ttype]).reshape(shape)
        self.tensors.append((name, list(shape), ttype, scales, zps, axis, data))
        return len(self.tensors) - 1

    def op(self, name, inputs, outputs, **options):
        self.ops.append((name, list(inputs), list(outputs), options))

    def build(self, inputs=None, outputs=None):
        """Returns the model as bytes. Unless you say otherwise, the inputs are
        the tensors nothing writes and the outputs the last operator's."""
        written = {t for _, _, outs, _ in self.ops for t in outs}
        if inputs is None:
            inputs = [i for i, t in enumerate(self.tensors)
                      if t[6] is None and i not in written]
        if outputs is None:
            outputs = self.ops[-1][2]
        b = flatbuffers.Builder(4096)
        Bf, T, QP = _B["Buffer"], _B["Tensor"], _B["QuantizationParameters"]

        # tensors without data all point at the empty buffer 0
        buffers, tbuf = [], []
        Bf.Start(b)
        buffers.append(Bf.End(b))
        for t in self.tensors:
            if t[6] is None:
                tbuf.append(0)
                continue
            dv = b.CreateByteVector(t[6].tobytes())
            Bf.Start(b)
            Bf.AddData(b, dv)
            buffers.append(Bf.End(b))
            tbuf.append(len(buffers) - 1)

        tens = []
        for i, (name, shape, ttype, scales, zps, axis, _) in enumerate(self.tensors):
            nm = b.CreateString(name)
            sh = _vector(b, T.StartShapeVector, shape, b.PrependInt32)
            q = None
            if scales is not None:
                sv = _vector(b, QP.StartScaleVector, scales, b.PrependFloat32)
                zv = _vector(b, QP.StartZeroPointVector,
                             [int(z) for z in (zps or [0] * len(scales))],
                             b.PrependInt64)
                QP.Start(b)
                QP.AddScale(b, sv)
                QP.AddZeroPoint(b, zv)
                QP.AddQuantizedDimension(b, axis)
                q = QP.End(b)
            T.Start(b)
            T.AddShape(b, sh)
            T.AddType(b, ttype)
            T.AddBuffer(b, tbuf[i])
            T.AddName(b, nm)
            if q is not None:
                T.AddQuantization(b, q)
            tens.append(T.End(b))

        codes = sorted({name for name, _, _, _ in self.ops})
        ops, O = [], _B["Operator"]
        for name, ins, outs, options in self.ops:
            table, member, _, defaults = OPTIONS[name]
            if table is not None:
                om = _mod(table)
                om.Start(b)
                for field, value in {**defaults, **options}.items():
                    getattr(om, "Add" + field)(b, value)
                opts = om.End(b)
            iv = _vector(b, O.StartInputsVector, ins, b.PrependInt32)
            ov = _vector(b, O.StartOutputsVector, outs, b.PrependInt32)
            O.Start(b)
            O.AddOpcodeIndex(b, codes.index(name))
            O.AddInputs(b, iv)
            O.AddOutputs(b, ov)
            if table is not None:
                O.AddBuiltinOptionsType(b, getattr(BuiltinOptions, member))
                O.AddBuiltinOptions(b, opts)
            ops.append(O.End(b))

        SG = _B["SubGraph"]
        tv = _vector(b, SG.StartTensorsVector, tens, b.PrependUOffsetTRelative)
        iv = _vector(b, SG.StartInputsVector, inputs, b.PrependInt32)
        ov = _vector(b, SG.StartOutputsVector, outputs, b.PrependInt32)
        opv = _vector(b, SG.StartOperatorsVector, ops, b.PrependUOffsetTRelative)
        SG.Start(b)
        SG.AddTensors(b, tv)
        SG.AddInputs(b, iv)
        SG.AddOutputs(b, ov)
        SG.AddOperators(b, opv)
        sg = SG.End(b)

        OC, M = _B["OperatorCode"], _B["Model"]
        ocs = []
        for name in codes:
            code = getattr(BuiltinOperator, name)
            OC.Start(b)
            OC.AddDeprecatedBuiltinCode(b, min(code, 127))
            OC.AddBuiltinCode(b, code)
            OC.AddVersion(b, OPTIONS[name][2])
            ocs.append(OC.End(b))
        ocv = _vector(b, M.StartOperatorCodesVector, ocs, b.PrependUOffsetTRelative)
        sgv = _vector(b, M.StartSubgraphsVector, [sg], b.PrependUOffsetTRelative)
        bv = _vector(b, M.StartBuffersVector, buffers, b.PrependUOffsetTRelative)
        M.Start(b)
        M.AddVersion(b, 3)
        M.AddOperatorCodes(b, ocv)
        M.AddSubgraphs(b, sgv)
        M.AddBuffers(b, bv)
        b.Finish(M.End(b), file_identifier=b"TFL3")
        return bytes(b.Output())


def run_reference(blob, feeds):
    """Runs the model once per input array and returns each first output. It
    asks for the reference kernels, since the optimised ones round differently."""
    from ai_edge_litert import interpreter as litert
    it = litert.Interpreter(
        model_content=blob,
        experimental_op_resolver_type=litert.OpResolverType.BUILTIN_REF)
    it.allocate_tensors()
    i_in = it.get_input_details()[0]["index"]
    i_out = it.get_output_details()[0]["index"]
    outs = []
    for x in feeds:
        it.set_tensor(i_in, x)
        it.invoke()
        outs.append(it.get_tensor(i_out).copy())
    return outs
