#!/usr/bin/env python3
# @file bench_run.py
# @brief Read the bench table from the board's USB CDC port into a results file
#
# Copyright (c) 2026 Antony Kasera
# SPDX-License-Identifier: MIT

import argparse
import datetime
import os
import sys
import termios
import time

BEGIN = "<!-- bench begin -->"
END = "<!-- bench end -->"


def open_port(path, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            return os.open(path, os.O_RDWR | os.O_NOCTTY)
        except OSError:
            time.sleep(0.1)  # not enumerated yet, or udev still setting up
    sys.exit(f"bench_run: could not open {path} within {timeout:.0f} s")


def set_raw(fd):
    a = termios.tcgetattr(fd)
    a[0] = 0                                          # iflag
    a[1] = 0                                          # oflag
    a[2] |= termios.CREAD | termios.CLOCAL | termios.CS8
    a[3] = 0                                          # lflag: no echo
    a[6][termios.VMIN] = 0
    a[6][termios.VTIME] = 1                           # read() returns after 0.1 s
    termios.tcsetattr(fd, termios.TCSANOW, a)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--out", required=True)
    ap.add_argument("--settle", type=float, default=1.5,
                    help="seconds to wait after flashing for re-enumeration")
    ap.add_argument("--timeout", type=float, default=180.0)
    ap.add_argument("--append", action="store_true",
                    help="add this image's block to --out")
    args = ap.parse_args()

    time.sleep(args.settle)  # else we may open the old, disconnecting device
    fd = open_port(args.port, 10.0)
    set_raw(fd)

    buf = ""
    deadline = time.monotonic() + args.timeout
    while END not in buf:
        if time.monotonic() > deadline:
            os.close(fd)
            sys.stdout.write(buf)
            sys.exit(f"\nbench_run: no '{END}' within {args.timeout:.0f} s")
        chunk = os.read(fd, 4096)
        if chunk:
            text = chunk.decode("utf-8", "replace")
            sys.stdout.write(text.replace("\r", ""))
            sys.stdout.flush()
            buf += text
    os.close(fd)

    start = buf.find(BEGIN)
    if start < 0:
        sys.exit(f"bench_run: '{END}' without '{BEGIN}'")
    block = buf[start:buf.find(END, start) + len(END)].replace("\r", "")

    today = datetime.date.today().isoformat()
    with open(args.out, "a" if args.append else "w") as f:
        if not args.append:
            f.write("# Bench results: STM32F103C8 (Blue Pill clone)\n\n")
            f.write(f"Written by `make bench-run` on {today}. Do not edit by "
                    "hand;\nrerun it. Method: [bench/README.md](../../bench/README.md).\n\n")
        else:
            f.write("\n")
        f.write(block + "\n")
    print(f"\nbench_run: wrote {args.out}")

    if "**FAIL**" in block:
        sys.exit("bench_run: golden mismatch, see the table")


if __name__ == "__main__":
    main()
