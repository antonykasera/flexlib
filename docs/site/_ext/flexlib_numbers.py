"""Numbers on the pages from the generated files, never typed.

{cy}`case` is the median of a bench case in docs/results/f103.md, warm, or
cold with `case @ cold`. {acc}`a; b; ...` is the max of the row of
docs/accuracy.md whose leading cells are a, b, ...; `@ column` names another
column. A table with a ratio column gets each ratio as the column before it
over the cycles column. A key that matches no row, or more than one, fails
the build.
"""

import re
from pathlib import Path

from docutils import nodes
from sphinx.util import logging

logger = logging.getLogger(__name__)

ROOT = Path(__file__).resolve().parents[3]
BENCH = ROOT / "docs/results/f103.md"
ACCURACY = ROOT / "docs/accuracy.md"

_cache = {}


def _tables(path):
    """Each markdown table of a file as (header, rows), header lowercased. A
    bare | in a case name (|x|, older bench files) stays in the name."""
    tables, header = [], None
    for line in path.read_text().splitlines():
        if not line.startswith("|"):
            header = None
            continue
        cells = re.split(r"(?<!\\)\|", line.strip()[1:-1])
        if header is not None and len(cells) > len(header):
            k = len(cells) - len(header) + 1
            cells[:k] = ["|".join(cells[:k])]
        cells = [c.strip().replace("\\|", "|") for c in cells]
        if header is None:
            header = [c.lower() for c in cells]
            tables.append((header, []))
        elif not set("".join(cells)) <= set("-: "):
            tables[-1][1].append(cells)
    return tables


def _data(path):
    mtime = path.stat().st_mtime
    if _cache.get(path, (None,))[0] != mtime:
        _cache[path] = (mtime, _tables(path))
    return _cache[path][1]


def bench(key):
    label, _, mode = (s.strip() for s in key.partition("@"))
    column = "median" if mode == "cold" else None
    hits = []
    for header, rows in _data(BENCH):
        if header[:2] != ["case", "impl"]:
            continue
        cold = "warm median" in header
        if (mode == "cold") != cold:
            continue
        col = header.index(column or "median")
        hits += [r[col] for r in rows if r[0] == label]
    return hits


def accuracy(key):
    parts, _, column = key.partition("@")
    parts = [p.strip() for p in parts.split(";")]
    column = column.strip().lower() or "max"
    hits = []
    for header, rows in _data(ACCURACY):
        if column not in header:
            continue
        col = header.index(column)
        hits += [r[col] for r in rows if r[:len(parts)] == parts]
    return hits


def number(text):
    """A cell as the page shows it: 4.04e+04 as 40400, 1.27e+07 as 1.27 10^7."""
    m = re.fullmatch(r"([+-]?[\d.]+)e([+-]\d+)", text)
    if not m:
        return text
    mant, exp = m.group(1), int(m.group(2))
    if 0 <= exp < 5:
        v = float(text)
        return str(int(v)) if v == int(v) else repr(v)
    return f"{mant} 10^{exp}"


def _role(lookup, path):
    def role(name, rawtext, text, lineno, inliner, options=None, content=None):
        env = inliner.document.settings.env
        env.note_dependency(str(path))
        hits = lookup(text)
        if len(hits) != 1:
            what = "no row" if not hits else f"{len(hits)} rows"
            msg = inliner.reporter.error(
                f"{name}: {what} for {text!r} in {path.relative_to(ROOT)}",
                line=lineno)
            return [inliner.problematic(rawtext, rawtext, msg)], [msg]
        shown = number(hits[0])
        node = nodes.inline(rawtext, shown, classes=["fl-data"])
        try:
            node["fl_value"] = float(hits[0])
        except ValueError:
            pass
        return [node], []
    return role


def _cell_value(entry):
    data = list(entry.findall(lambda n: isinstance(n, nodes.inline)
                              and "fl-data" in n["classes"]))
    return data[0].get("fl_value") if len(data) == 1 else None


def ratios(doctree):
    for table in doctree.findall(nodes.table):
        head = next(table.findall(nodes.thead), None)
        body = next(table.findall(nodes.tbody), None)
        if head is None or body is None:
            continue
        names = [e.astext().strip().lower() for e in head.findall(nodes.entry)]
        if "ratio" not in names or "cycles" not in names:
            continue
        r, c = names.index("ratio"), names.index("cycles")
        for row in body.findall(nodes.row):
            entries = list(row.findall(nodes.entry))
            if entries[r].astext().strip():
                logger.warning("ratio typed in a table; leave it empty",
                               location=entries[r])
                continue
            base, ours = _cell_value(entries[r - 1]), _cell_value(entries[c])
            if base is not None and ours:
                para = next(entries[r].findall(nodes.paragraph), None)
                if para is None:
                    para = nodes.paragraph()
                    entries[r] += para
                para += nodes.Text(f"{base / ours:.2f}")


def setup(app):
    app.add_role("cy", _role(bench, BENCH))
    app.add_role("acc", _role(accuracy, ACCURACY))
    app.connect("doctree-read", lambda app, doctree: ratios(doctree))
    return {"parallel_read_safe": True, "parallel_write_safe": True}
