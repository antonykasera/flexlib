"""ASCII math to typeset nodes: superscripts and symbols in prose, $...$ formulas.

The sources (headers and pages) keep plain ASCII, which reads well in an
editor: 2^32, 10^-6, +-, <=, x'x. A formula or domain between dollar signs,
$y = b0 x + a1 y1$, becomes a tinted span with italic variables and
subscripted indices.
"""

import re

from docutils import nodes
from sphinx import addnodes
from sphinx.util import logging

logger = logging.getLogger(__name__)

MINUS = "−"

# an exponent: a parenthesised group, a signed number or a signed name
_EXP = r"\((?:[^()]|\([^()]*\))*\)|-?\d+(?:\.\d+)?|-?[A-Za-z]\w*"

_PROSE = re.compile(
    r"(?<=[\w|)\]])\^(?P<sup>" + _EXP + r")"
    r"|(?<![\w.])(?P<a>[1-9]\d*)(?P<s1> ?)x(?P<s2> ?)(?=[1-9]\d*(?![\w.]))"
    r"|(?<=\d) (?=10\^)"                      # 5 10^-4: a product
    r"|(?P<sym>\+-|<=|>=|!=)"
    r"|(?P<pi>\bpi\b)"
    r"|(?P<sqrt>\bsqrt )(?=[a-z]\b|\d|\()"
    r"|\b(?P<tr>[a-z])'(?P=tr)\b"             # x'x: x transposed times x
)

_FORMULA = re.compile(
    r"(?<=[\w|)\]])\^(?P<sup>" + _EXP + r")"
    r"|(?<=[A-Za-z\])])'(?=[A-Za-z(])"        # transpose
    r"|(?<=[A-Za-z0-9])_(?P<sub>[A-Za-z0-9]+)" # a_i, out_min
    r"|(?P<id>[A-Za-z]+\d*)"
    r"|(?P<num>\d+(?:\.\d+)?)"
    r"|(?P<op><=|>=|!=|\+-|->|~|-)"
)

_SYMBOLS = {"+-": "±", "<=": "≤", ">=": "≥", "!=": "≠",
            "->": "→", "~": "≈", "-": MINUS}
_WORDS = {"pi": "π", "inf": "∞", "sum": "Σ", "sqrt": "√"}
_GREEK = {"alpha": "α", "beta": "β", "mu": "μ", "rho": "ρ",
          "sigma": "σ", "tau": "τ", "theta": "θ", "omega": "ω"}
_UPRIGHT = {"sin", "cos", "tan", "atan", "atan2", "asin", "acos", "exp",
            "exp2", "ln", "log", "round", "sign", "max", "min", "mod", "floor",
            "ceil", "abs", "Re", "Im"}
_INDEXED = re.compile(r"([A-Za-z]+?)(\d+)")
_SKIP = (nodes.literal, nodes.FixedTextElement, nodes.raw, nodes.comment,
         nodes.math, addnodes.desc_signature)


def _strip_parens(s):
    return s[1:-1] if s.startswith("(") and s.endswith(")") else s


def _sup(children):
    return nodes.superscript("", "", *children)


def _merge(out):
    """Adjacent Text pieces as one node."""
    merged = []
    for n in out:
        if isinstance(n, nodes.Text) and merged and isinstance(merged[-1], nodes.Text):
            merged[-1] = nodes.Text(str(merged[-1]) + str(n))
        else:
            merged.append(n)
    return merged


def prose(text):
    out, pos = [], 0
    for m in _PROSE.finditer(text):
        out.append(nodes.Text(text[pos:m.start()]))
        pos = m.end()
        if m.group("sup") is not None:
            inner = _strip_parens(m.group("sup"))
            if inner.startswith("-"):
                inner = MINUS + inner[1:]
            out.append(_sup(prose(inner)))
        elif m.group("a") is not None:
            gap = " " if m.group("s1") or m.group("s2") else ""  # no break
            out.append(nodes.Text(m.group("a") + gap + "×" + gap))
        elif m.group("sym") is not None:
            out.append(nodes.Text(_SYMBOLS[m.group("sym")]))
        elif m.group("pi") is not None:
            out.append(nodes.Text(_WORDS["pi"]))
        elif m.group("sqrt") is not None:
            out.append(nodes.Text(_WORDS["sqrt"]))
        elif m.group("tr") is not None:
            v = m.group("tr")
            out += [nodes.Text(v), _sup([nodes.Text("T")]), nodes.Text(v)]
        else:
            out.append(nodes.Text(" × "))
    out.append(nodes.Text(text[pos:]))
    return _merge([n for n in out if not (isinstance(n, nodes.Text) and not str(n))])


def _identifier(word):
    if word in _WORDS:
        return [nodes.Text(_WORDS[word])]
    if word in _GREEK:
        return [nodes.inline("", _GREEK[word], classes=["fl-var"])]
    if word in ("log2", "log10"):
        return [nodes.Text("log"), nodes.subscript("", word[3:])]
    if word in _UPRIGHT or (len(word) > 1 and word.isupper()):  # LSB, ULP
        return [nodes.Text(word)]
    m = _INDEXED.fullmatch(word)
    if m:
        return [nodes.inline("", m.group(1), classes=["fl-var"]),
                nodes.subscript("", m.group(2))]
    return [nodes.inline("", word, classes=["fl-var"])]


def formula(text):
    out, pos = [], 0
    for m in _FORMULA.finditer(text):
        out.append(nodes.Text(text[pos:m.start()]))
        pos = m.end()
        if m.group("sup") is not None:
            inner = _strip_parens(m.group("sup"))
            out.append(_sup(formula(inner)))
        elif m.group("sub") is not None:
            out.append(nodes.subscript("", m.group("sub")))
        elif m.group("id") is not None:
            out += _identifier(m.group("id"))
        elif m.group("num") is not None:
            out.append(nodes.Text(m.group("num")))
        elif m.group("op") is not None:
            out.append(nodes.Text(_SYMBOLS[m.group("op")]))
        else:
            out.append(_sup([nodes.Text("T")]))
    out.append(nodes.Text(text[pos:]))
    return _merge([n for n in out if not (isinstance(n, nodes.Text) and not str(n))])


def _skipped(node):
    while node is not None:
        if isinstance(node, _SKIP):
            return True
        node = node.parent
    return False


def typeset(doctree):
    for t in list(doctree.findall(nodes.Text)):
        parent = t.parent
        if parent is None or _skipped(parent):
            continue
        s = str(t)
        if s.count("$") % 2:
            logger.warning("unpaired $ in %r", s, location=parent)
            continue
        out = []
        for i, part in enumerate(re.split(r"\$([^$]+)\$", s)):
            if i % 2:
                out.append(nodes.inline("", "", *formula(part), classes=["fl-math"]))
            elif part:
                out += prose(part)
        if len(out) == 1 and isinstance(out[0], nodes.Text) and str(out[0]) == s:
            continue
        parent.replace(t, out)
