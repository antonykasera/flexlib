#!/usr/bin/env python3
# @file bench_run.py
# @brief Read the bench table from the board (USB CDC, or RTT through OpenOCD)
# into a results file
#
# Copyright (c) 2026 Antony Kasera
# SPDX-License-Identifier: Apache-2.0

import argparse
import datetime
import os
import shlex
import socket
import subprocess
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


def rtt_address(nm, elf):
    out = subprocess.run([nm, elf], capture_output=True, text=True,
                         check=True).stdout
    for line in out.splitlines():
        f = line.split()
        if len(f) == 3 and f[2] == "bench_rtt":
            return int(f[0], 16)
    sys.exit(f"bench_run: no bench_rtt in {elf}: not an RTT build")


def start_openocd(args):
    """Flash, reset, serve the RTT up-buffer on args.rtt_port."""
    addr = rtt_address(args.nm, args.rtt)
    cmds = ["init",
            f"program {{{args.rtt}}} verify reset",
            "sleep 100",  # the control block is in .data: set up after reset
            f"rtt setup 0x{addr:08x} 16 {{SEGGER RTT}}",
            "rtt polling_interval 5",
            "rtt start",
            f"rtt server start {args.rtt_port} 0"]
    argv = ["openocd", *shlex.split(args.openocd)]
    for c in cmds:
        argv += ["-c", c]
    log = open(os.path.join(os.path.dirname(args.rtt), "openocd.log"), "w")
    return subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT), log


def connect(port, ocd, log, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if ocd.poll() is not None:
            sys.exit(f"bench_run: openocd exited, see {log.name}")
        try:
            sock = socket.create_connection(("localhost", port), timeout=1.0)
            sock.settimeout(0.1)
            return sock
        except OSError:
            time.sleep(0.2)  # still flashing
    sys.exit(f"bench_run: no RTT server on port {port}, see {log.name}")


def read_report(read, timeout):
    """Text from read() up to the end marker; read() returns b"" when idle."""
    buf = ""
    deadline = time.monotonic() + timeout
    while END not in buf:
        if time.monotonic() > deadline:
            sys.stdout.write(buf)
            sys.exit(f"\nbench_run: no '{END}' within {timeout:.0f} s")
        chunk = read()
        if chunk:
            text = chunk.decode("utf-8", "replace")
            sys.stdout.write(text.replace("\r", ""))
            sys.stdout.flush()
            buf += text
    return buf


def read_usb(args):
    time.sleep(args.settle)  # else we may open the old, disconnecting device
    fd = open_port(args.port, 10.0)
    set_raw(fd)
    try:
        return read_report(lambda: os.read(fd, 4096), args.timeout)
    finally:
        os.close(fd)


def read_rtt(args):
    ocd, log = start_openocd(args)
    try:
        sock = connect(args.rtt_port, ocd, log, 60.0)

        def read():
            try:
                chunk = sock.recv(4096)
            except socket.timeout:
                return b""
            if not chunk:
                sys.exit(f"bench_run: openocd closed the connection, see {log.name}")
            return chunk

        try:
            return read_report(read, args.timeout)
        finally:
            sock.close()
    finally:
        ocd.terminate()
        ocd.wait()
        log.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--out", required=True)
    ap.add_argument("--settle", type=float, default=1.5,
                    help="seconds to wait after flashing for re-enumeration")
    ap.add_argument("--timeout", type=float, default=180.0)
    ap.add_argument("--append", action="store_true",
                    help="add this image's block to --out")
    ap.add_argument("--rtt", metavar="ELF",
                    help="flash ELF with OpenOCD and read its RTT buffer "
                         "instead of --port")
    ap.add_argument("--openocd", default="-f interface/stlink.cfg "
                    "-f target/stm32f1x.cfg", help="OpenOCD board arguments")
    ap.add_argument("--nm", default="arm-none-eabi-nm")
    ap.add_argument("--rtt-port", type=int, default=9090)
    ap.add_argument("--title", default="STM32F103C8 (Blue Pill clone)",
                    help="the board, for the file's heading")
    args = ap.parse_args()

    buf = read_rtt(args) if args.rtt else read_usb(args)

    start = buf.find(BEGIN)
    if start < 0:
        sys.exit(f"bench_run: '{END}' without '{BEGIN}'")
    block = escape_pipes(buf[start:buf.find(END, start) + len(END)]
                         .replace("\r", ""))

    today = datetime.date.today().isoformat()
    with open(args.out, "a" if args.append else "w") as f:
        if not args.append:
            f.write(f"# Bench results: {args.title}\n\n")
            f.write(f"Written by `make bench-run` on {today}. Do not edit by "
                    "hand;\nrerun it. Method: [bench/README.md](https://github.com/antonykasera/flexlib/blob/master/bench/README.md).\n\n")
        else:
            f.write("\n")
        f.write(block + "\n")
    print(f"\nbench_run: wrote {args.out}")

    if "**FAIL**" in block:
        sys.exit("bench_run: golden mismatch, see the table")


def escape_pipes(block):
    """A case name with | in it (|x|) as \\| so the row keeps its columns."""
    out, width = [], None
    for line in block.split("\n"):
        if not line.startswith("|"):
            width = None
        else:
            cells = line[1:-1].split("|")
            if width is None:
                width = len(cells)
            elif len(cells) > width:
                k = len(cells) - width + 1
                line = "|" + "\\|".join(cells[:k]) + "|" + "|".join(cells[k:]) + "|"
        out.append(line)
    return "\n".join(out)


if __name__ == "__main__":
    main()
