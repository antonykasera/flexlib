"""Numbers on the pages from the generated files, never typed.

{cy}`case` is the median of a bench case in docs/results/f103.md, warm, or
cold with `case @ cold`. {acc}`a; b; ...` is the max of the row of
docs/accuracy.md whose leading cells are a, b, ...; `@ column` names another
column. A table with a ratio column gets each ratio as the column before it
over the cycles column. A key that matches no row, or more than one, fails
the build. {bm}`build` is the rest of a line of the bench conditions.

The bench-rows directive lists the rows of one header's functions, and of
the cases whose names start with a line of its content (a line ending in $
must match whole). Every row but candidates and kernels must appear on
exactly one page.
"""

import re
import xml.etree.ElementTree as ET
from pathlib import Path

from docutils import nodes
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective

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


def bench_meta(key):
    """The rest of the first `- key ...` line of the bench file."""
    for line in BENCH.read_text().splitlines():
        if line.startswith(f"- {key}"):
            return [line[len(key) + 2:].lstrip(": ")]
    return []


# ---- results pages

SKIP_IMPL = {"candidate", "kernel"}
_FIRST = re.compile(r"f[udmx]_[a-z0-9_]+")


def bench_rows():
    """(id, cells, cold) for every case row; id is its place in the file."""
    out = []
    for t, (header, rows) in enumerate(_data(BENCH)):
        if header[:2] != ["case", "impl"]:
            continue
        cold = "warm median" in header
        out += [((t, r), dict(zip(header, cells)), cold)
                for r, cells in enumerate(rows)]
    return out


def load_owners(app):
    """Function name to the header that declares it, from the Doxygen XML."""
    xml_dir = Path(app.config.breathe_projects[app.config.breathe_default_project])
    owners = {}
    for f in xml_dir.glob("*_8h.xml"):
        root = ET.parse(f).getroot()
        name = root.findtext("compounddef/compoundname")
        for m in root.iter("memberdef"):
            if m.get("kind") == "function":
                owners[m.findtext("name")] = name
    app.env.fl_owners = owners


def _case_cell(case):
    m = _FIRST.match(case)
    para = nodes.paragraph()
    if m:
        para += nodes.literal(m.group(0), m.group(0), classes=["fl-fn"])
        para += nodes.Text(case[m.end():])
    else:
        para += nodes.Text(case)
    return para


def _table(columns, rows):
    table = nodes.table(classes=["fl-bench"])
    group = nodes.tgroup(cols=len(columns))
    table += group
    for _ in columns:
        group += nodes.colspec(colwidth=1)
    head, body = nodes.thead(), nodes.tbody()
    group += head
    group += body
    row = nodes.row()
    head += row
    for title, _key, numeric in columns:
        entry = nodes.entry(classes=["text-right"] if numeric else [])
        entry += nodes.paragraph(text=title)
        row += entry
    for cells in rows:
        row = nodes.row()
        body += row
        for _title, key, numeric in columns:
            entry = nodes.entry(classes=["text-right"] if numeric else [])
            entry += (_case_cell(cells[key]) if key == "case"
                      else nodes.paragraph(text=cells[key]))
            row += entry
    return table


WARM = [("case", "case", False), ("impl", "impl", False),
        ("min", "min", True), ("median", "median", True), ("max", "max", True)]
COLD = [("case", "case", False), ("impl", "impl", False),
        ("warm", "warm median", True), ("cold min", "min", True),
        ("cold median", "median", True), ("cold max", "max", True)]


class BenchRows(SphinxDirective):
    optional_arguments = 1
    has_content = True

    def run(self):
        env = self.env
        env.note_dependency(str(BENCH))
        header = self.arguments[0] if self.arguments else None
        lines = [l.strip() for l in self.content if l.strip()]
        used = set()
        owners = getattr(env, "fl_owners", {})
        warm, cold = [], []
        claims = env.fl_claims.setdefault(env.docname, [])
        for rid, cells, is_cold in bench_rows():
            if cells["impl"] in SKIP_IMPL:
                continue
            case = cells["case"]
            m = _FIRST.match(case)
            hit = [l for l in lines if (case == l[:-1] if l.endswith("$")
                                        else case.startswith(l))]
            used.update(hit)
            if not hit and not (header and m and owners.get(m.group(0)) == header):
                continue
            (cold if is_cold else warm).append(cells)
            claims.append(rid)
        for l in lines:
            if l not in used:
                logger.warning(f"bench-rows: no row for {l!r}",
                               location=self.get_location())
        if not warm and not cold:
            logger.warning(f"bench-rows: no rows for {header!r}",
                           location=self.get_location())
        out = [_table(WARM, warm)] if warm else []
        if cold:
            para = nodes.paragraph(text="Cold: the flash cache flushed before "
                                        "each call.")
            out += [para, _table(COLD, cold)]
        return out


def purge_claims(app, env, docname):
    getattr(env, "fl_claims", {}).pop(docname, None)


def merge_claims(app, env, docnames, other):
    for d in docnames:
        if d in getattr(other, "fl_claims", {}):
            env.fl_claims[d] = other.fl_claims[d]


def init_claims(app, env, docnames):
    if not hasattr(env, "fl_claims"):
        env.fl_claims = {}


def check_claims(app, env):
    count = {}
    for ids in env.fl_claims.values():
        for rid in ids:
            count[rid] = count.get(rid, 0) + 1
    for rid, cells, _cold in bench_rows():
        if cells["impl"] in SKIP_IMPL or count.get(rid, 0) == 1:
            continue
        logger.warning(f"bench row {cells['case']!r} ({cells['impl']}) is on "
                       f"{count.get(rid, 0)} results tables, not 1")


def setup(app):
    app.add_role("cy", _role(bench, BENCH))
    app.add_role("acc", _role(accuracy, ACCURACY))
    app.add_role("bm", _role(bench_meta, BENCH))
    app.add_directive("bench-rows", BenchRows)
    app.connect("builder-inited", load_owners)
    app.connect("env-before-read-docs", init_claims)
    app.connect("env-purge-doc", purge_claims)
    app.connect("env-merge-info", merge_claims)
    app.connect("env-check-consistency", check_claims)
    app.connect("doctree-read", lambda app, doctree: ratios(doctree))
    return {"parallel_read_safe": True, "parallel_write_safe": True}
