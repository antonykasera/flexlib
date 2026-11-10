#!/usr/bin/env python3
# @file bench_windows.py
# @brief List the instructions each bench case times
#
# Copyright (c) 2026 Antony Kasera
# SPDX-License-Identifier: MIT

import argparse
import re
import subprocess
import sys

FUNC_RE = re.compile(r"^[0-9a-f]+ <([^>]+)>:$")
LINE_RE = re.compile(r"^\s+([0-9a-f]+):\s+(\S+)\s*(.*)$")
RELOC_RE = re.compile(r"R_ARM_\w+\s+(\S+)")
LIT_LOAD_RE = re.compile(r"(r\d+|ip|lr|sl|fp), \[pc, #\d+\]\s+@ \(?([0-9a-f]+)")
MEM_RE = re.compile(r"\[(r\d+|ip|lr|sl|fp)(?:, #(\d+))?\]$")
DWT_CYCCNT = 0xE0001004


def functions(objdump, objs):
    out = subprocess.run([objdump, "-dr", "--no-show-raw-insn", *objs],
                         check=True, capture_output=True, text=True).stdout
    funcs, cur = [], None
    for line in out.splitlines():
        m = FUNC_RE.match(line)
        if m:
            cur = (m.group(1), [])
            funcs.append(cur)
        elif cur is not None and line.strip():
            cur[1].append(line)
    return funcs


def windows(lines):
    lits = {}
    for line in lines:
        m = LINE_RE.match(line)
        if m and m.group(2) == ".word":
            lits[int(m.group(1), 16)] = int(m.group(3).split()[0], 16)

    regs, reads, insns = {}, [], []
    for line in lines:
        r = RELOC_RE.search(line)
        if r:
            if insns:  # objdump's own <name> is wrong before relocation
                insns[-1] = re.sub(r" <[^>]*>$", "", insns[-1])
                insns[-1] += " <" + r.group(1) + ">"
            continue
        m = LINE_RE.match(line)
        if not m or m.group(2).startswith("."):
            continue
        op, args = m.group(2), m.group(3).split("@")[0].strip()
        dst = args.split(",")[0]
        lit = LIT_LOAD_RE.search(m.group(3))
        if op.startswith("ldr") and lit:
            regs[lit.group(1)] = lits.get(int(lit.group(2), 16))
        elif op.startswith("movw"):
            regs[dst] = int(args.split("#")[1])
        elif op.startswith("movt") and regs.get(dst) is not None:
            regs[dst] = (regs[dst] & 0xFFFF) | (int(args.split("#")[1]) << 16)
        mem = MEM_RE.search(args)
        if op.startswith("ldr") and mem and regs.get(mem.group(1)) is not None:
            if regs[mem.group(1)] + int(mem.group(2) or 0) == DWT_CYCCNT:
                reads.append(len(insns))
        insns.append(f"{op} {args}".strip())

    return [insns[a + 1:b] for a, b in zip(reads[::2], reads[1::2])], len(reads)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--objdump", default="arm-none-eabi-objdump")
    ap.add_argument("objs", nargs="+")
    args = ap.parse_args()

    bad = 0
    for name, lines in functions(args.objdump, args.objs):
        wins, nreads = windows(lines)
        if nreads == 0:
            continue
        if nreads != 2:
            print(f"  {name}: {nreads} CYCCNT reads (expected 2)")
            bad += 1
        for w in wins:
            print(f"  {name:20} {len(w):2} | " + " ; ".join(w))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
