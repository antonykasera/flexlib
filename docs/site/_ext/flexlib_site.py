"""Site helpers: macro values in API entries, C classes on inline code, math."""

import re
import xml.etree.ElementTree as ET
from pathlib import Path

from docutils import nodes
from sphinx import addnodes

import flexlib_math

C_KEYWORDS = {
    "auto", "break", "case", "const", "continue", "default", "do", "else",
    "enum", "extern", "for", "goto", "if", "inline", "register", "restrict",
    "return", "sizeof", "static", "struct", "switch", "typedef", "union",
    "volatile", "while", "NULL", "true", "false",
}
C_TYPES = {
    "void", "char", "short", "int", "long", "unsigned", "signed", "float",
    "double", "bool", "_Bool", "size_t", "uintptr_t", "ptrdiff_t",
}

_TYPE = re.compile(r"u?int(8|16|32|64)_t|fm_q(15|31|16)|fd_c(q15|q31|f32)|"
                   r"F[UMDX]_[A-Z][a-z]\w*")
_STRING = re.compile(r'"[^"]*"|\'.\'')
_NUMBER = re.compile(r"-?(0[xX][0-9A-Fa-f]+|\d+(\.\d+)?)[uUlLfF]*")
_MACRO = re.compile(r"_?[A-Z][A-Z0-9_]*(\(.*\))?")
_FUNCTION = re.compile(r"[A-Za-z_]\w*\(\)|f[umdx]_[a-z0-9_]+")

# constant macros only: a function-like macro's body is implementation
_SHORT_VALUE = 24
# set per target; Doxygen only sees the host's value
_TARGET_MACROS = {"FX_ARM", "FX_HAVE_DSP", "FX_HAVE_FPU"}


def classify(text):
    if text in C_KEYWORDS:
        return "fl-kw"
    if text in C_TYPES or _TYPE.fullmatch(text):
        return "fl-type"
    if _STRING.fullmatch(text):
        return "fl-str"
    if _NUMBER.fullmatch(text):
        return "fl-num"
    if _MACRO.fullmatch(text):
        return "fl-macro"
    if _FUNCTION.fullmatch(text):
        return "fl-fn"
    return None


def load_macro_values(app):
    values = {}
    xml_dir = Path(app.config.breathe_projects[app.config.breathe_default_project])
    for f in xml_dir.glob("*_8h.xml"):
        for m in ET.parse(f).iter("memberdef"):
            if m.get("kind") != "define" or m.find("param") is not None:
                continue
            init = "".join(m.find("initializer").itertext()).strip() \
                if m.find("initializer") is not None else ""
            if init and len(init) <= _SHORT_VALUE and \
                    m.findtext("name") not in _TARGET_MACROS:
                values[m.findtext("name")] = init
    app.env.fl_macro_values = values


def in_signature(node):
    while node is not None:
        if isinstance(node, addnodes.desc_signature):
            return True
        node = node.parent
    return False


def add_macro_values(doctree, values):
    for desc in doctree.findall(addnodes.desc):
        if desc.get("domain") != "c" or desc.get("objtype") != "macro":
            continue
        for sig in desc.findall(addnodes.desc_signature):
            name = next(sig.findall(addnodes.desc_name), None)
            value = values.get(name.astext()) if name is not None else None
            if value is None:
                continue
            line = list(sig.findall(addnodes.desc_signature_line)) or [sig]
            line[0].insert(0, addnodes.desc_sig_keyword("#define ", "#define ",
                                                         classes=["fl-pp"]))
            line[-1] += addnodes.desc_sig_space()
            line[-1] += addnodes.desc_sig_literal_number(value, value,
                                                         classes=["fl-val"])


_PARAM_SEP = ("--", "\u2013", "\u2014")


def param_columns(doctree):
    """`name -- text` items of a Parameters or Return values field into name
    and text parts."""
    for field in doctree.findall(nodes.field):
        if not field[0].astext().startswith(("Parameters", "Return values")):
            continue
        for p in list(field[1].findall(nodes.paragraph)):
            if not p.children or not isinstance(p[0], nodes.strong):
                continue
            name, rest = p[0], list(p.children[1:])
            if rest and isinstance(rest[0], nodes.Text):
                text = str(rest[0]).lstrip()
                for sep in _PARAM_SEP:
                    if text.startswith(sep):
                        text = text[len(sep):].lstrip()
                        break
                rest[0] = nodes.Text(text)
            p.children = []
            p += nodes.literal(name.astext(), name.astext(), classes=["fl-pname"])
            p += nodes.inline("", "", *rest, classes=["fl-pdesc"])
            p["classes"].append("fl-param")


def on_doctree_resolved(app, doctree, docname):
    add_macro_values(doctree, getattr(app.env, "fl_macro_values", {}))
    param_columns(doctree)
    # Doxygen's links to functions arrive as plain text: make them code
    for ref in list(doctree.findall(nodes.reference)):
        for inl in list(ref.findall(nodes.inline)):
            text = inl.astext()
            if "std-ref" in inl["classes"] and _FUNCTION.fullmatch(text):
                inl.replace_self(nodes.literal(text, text))
    for lit in doctree.findall(nodes.literal):
        if in_signature(lit):
            continue
        cls = classify(lit.astext())
        if cls:
            lit["classes"].append(cls)
    flexlib_math.typeset(doctree)


def setup(app):
    app.connect("builder-inited", load_macro_values)
    app.connect("doctree-resolved", on_doctree_resolved)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
