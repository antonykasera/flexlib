#!/usr/bin/env python3
# @file asm_audit.py
# @brief Check hot kernels' disassembly for expected instructions and for
#        calls to compiler helpers (soft-float, libgcc)
#
# Copyright (c) 2026 Antony Kasera
# SPDX-License-Identifier: MIT
#
# usage: asm_audit.py --spec test/asm/audit.txt [--objdump X] obj.o...

import argparse
import re
import subprocess
import sys

FUNC_RE = re.compile(r"^[0-9a-f]+ <([^>]+)>:$")
INSN_RE = re.compile(r"^\s+[0-9a-f]+:\s+(\S+)")
CALL_RELOC_RE = re.compile(
    r"R_ARM_(?:THM_CALL|THM_JUMP24|THM_JUMP19|CALL|JUMP24)\s+(\S+)")
CONDS = {"eq", "ne", "cs", "cc", "hs", "lo", "mi", "pl", "vs", "vc", "hi",
         "ls", "ge", "lt", "gt", "le", "al"}


def base(mnemonic):
    return re.sub(r"\.(w|n)$", "", mnemonic)


def matches(mnemonic, want):
    # exact, or with a flag-setting `s` and/or condition suffix (IT blocks)
    m = base(mnemonic)
    if not m.startswith(want):
        return False
    rest = m[len(want):]
    if rest.startswith("s"):
        rest = rest[1:]
    return rest == "" or rest in CONDS


def parse_spec(path):
    spec = []
    with open(path) as f:
        for n, line in enumerate(f, 1):
            line = line.split("#", 1)[0].split()
            if not line:
                continue
            entry = {"sym": line[0], "max": None, "calls_ok": False,
                     "no_calls": False, "want": [], "never": [], "line": n}
            for tok in line[1:]:
                if tok.startswith("max="):
                    entry["max"] = int(tok[4:])
                elif tok == "calls-ok":
                    entry["calls_ok"] = True
                elif tok == "no-calls":
                    entry["no_calls"] = True
                elif tok.startswith("!"):
                    entry["never"].append(tok[1:])
                else:
                    entry["want"].append(tok)
            spec.append(entry)
    return spec


def disassemble(objdump, objs):
    out = subprocess.run([objdump, "-dr", "--no-show-raw-insn", *objs],
                         check=True, capture_output=True, text=True).stdout
    funcs = {}
    cur = None
    for line in out.splitlines():
        m = FUNC_RE.match(line)
        if m:
            cur = funcs.setdefault(m.group(1), {"insns": [], "calls": []})
            continue
        if cur is None:
            continue
        r = CALL_RELOC_RE.search(line)
        if r:
            cur["calls"].append(r.group(1))
            continue
        i = INSN_RE.match(line)
        if i and not i.group(1).startswith("."):  # skip .word literal pools
            cur["insns"].append(i.group(1))
    for f in funcs.values():
        while f["insns"] and f["insns"][-1] == "nop":  # alignment padding
            f["insns"].pop()
    return funcs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True)
    ap.add_argument("--objdump", default="arm-none-eabi-objdump")
    ap.add_argument("objs", nargs="+")
    args = ap.parse_args()

    funcs = disassemble(args.objdump, args.objs)
    failed = 0
    spec = parse_spec(args.spec)
    for e in spec:
        f = funcs.get(e["sym"])
        problems = []
        if f is None:
            problems.append("symbol not found")
        else:
            missing = [w for w in e["want"]
                       if not any(matches(i, w) for i in f["insns"])]
            if missing:
                problems.append("missing " + " ".join(missing))
            banned = sorted({i for i in f["insns"]
                             for w in e["never"] if matches(i, w)})
            if banned:
                problems.append("has " + " ".join(banned))
            if e["max"] is not None and len(f["insns"]) > e["max"]:
                problems.append(f"{len(f['insns'])} insns > max {e['max']}")
            if e["no_calls"] and f["calls"]:
                problems.append("calls " + " ".join(sorted(set(f["calls"]))))
            helpers = [c for c in f["calls"] if c.startswith("__")]
            if helpers and not e["calls_ok"]:
                problems.append("calls " + " ".join(sorted(set(helpers))))
        n = "-" if f is None else str(len(f["insns"]))
        status = "FAIL" if problems else "ok"
        detail = "; ".join(problems) if problems else " ".join(
            e["want"] + ["!" + w for w in e["never"]])
        print(f"  {status:4}  {e['sym']:24} {n:>4} insns  {detail}")
        failed += bool(problems)

    print(f"asm-audit: {len(spec)} checked, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
