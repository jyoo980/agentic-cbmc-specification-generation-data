"""Parallel mutation kill-score for EncodeTree using the real run_cbmc pipeline.

Each mutant is applied to a private copy of zopfli.c in its own temp dir (with a
copy of zopfli.h alongside so the relative #include resolves), and verified with
tools.run_cbmc.run_cbmc (same unwind/depth/stubs as the avocado ground truth).
A mutant is KILLED iff the mutated program does not verify (and is not a goto-cc
build failure, which is excluded).

Run from anywhere with the venv python; it self-inserts /app on sys.path.
"""
import os
import sys
# Drop this script's own dir (c_code) from sys.path BEFORE importing stdlib modules
# (tempfile/random import `bisect`, which the local bisect.py would otherwise shadow).
_here = os.path.dirname(os.path.abspath(__file__))
sys.path[:] = [p for p in sys.path if os.path.abspath(p or os.getcwd()) != _here]
sys.path.insert(0, "/app")

import re
import shutil
import subprocess
import tempfile
from concurrent.futures import ProcessPoolExecutor

from tools.run_cbmc import run_cbmc

CDIR = "/app/Syzygy_Zopfli/c_code"
SRC = os.path.join(CDIR, "zopfli.c")
HDR = os.path.join(CDIR, "zopfli.h")
FUNC = "EncodeTree"


def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FUNC, "--file", SRC],
        capture_output=True, text=True,
    ).stdout
    mutants = []
    for block in out.split("--- original")[1:]:
        m = re.search(r"@@ -(\d+) \+(\d+) @@", block)
        if not m:
            continue
        lineno = int(m.group(1))
        orig_line = mut_line = None
        for ln in block.splitlines():
            if ln.startswith("-") and not ln.startswith("---"):
                orig_line = ln[1:]
            elif ln.startswith("+") and not ln.startswith("+++"):
                mut_line = ln[1:]
        if orig_line is not None and mut_line is not None:
            mutants.append((lineno, orig_line, mut_line))
    return mutants


ORIG_LINES = open(SRC).read().split("\n")


def run_one(arg):
    idx, lineno, o, m = arg
    cur = ORIG_LINES[lineno - 1]
    tag = "L%d#%d" % (lineno, idx)
    if cur.strip() != o.strip():
        return (tag, "SKIP", o.strip(), m.strip())
    d = tempfile.mkdtemp(prefix="km_%d_" % idx)
    try:
        shutil.copy(HDR, os.path.join(d, "zopfli.h"))
        lines = list(ORIG_LINES)
        lines[lineno - 1] = m
        src = os.path.join(d, "zopfli.c")
        open(src, "w").write("\n".join(lines))
        r = run_cbmc(FUNC, src, cwd=d)
        if r.failed_step is not None and r.failed_step.value == "goto-cc":
            return (tag, "BUILDFAIL", o.strip(), m.strip())
        return (tag, "KILLED" if not r.is_function_verified else "SURVIVED", o.strip(), m.strip())
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main():
    mutants = get_mutants()
    args = [(i, ln, o, m) for i, (ln, o, m) in enumerate(mutants)]
    print("Found %d mutants\n" % len(mutants), flush=True)
    killed = survived = buildfail = skip = 0
    survivors = []
    with ProcessPoolExecutor(max_workers=8) as ex:
        for tag, verdict, o, m in ex.map(run_one, args):
            if verdict == "SKIP":
                print("%-12s SKIP (line mismatch)" % tag, flush=True); skip += 1
            elif verdict == "BUILDFAIL":
                print("%-12s BUILD-FAIL (excluded) | %s" % (tag, m), flush=True); buildfail += 1
            elif verdict == "SURVIVED":
                print("%-12s SURVIVED | %s -> %s" % (tag, o, m), flush=True)
                survived += 1; survivors.append((tag, o, m))
            else:
                print("%-12s KILLED   | %s -> %s" % (tag, o, m), flush=True); killed += 1
    total = killed + survived
    print("\nKILLED=%d SURVIVED=%d BUILD-FAIL=%d SKIP=%d  kill score = %d/%d"
          % (killed, survived, buildfail, skip, killed, total if total else 0), flush=True)
    if survivors:
        print("\nSurvivors:")
        for tag, o, m in survivors:
            print("  %-12s %s -> %s" % (tag, o, m), flush=True)


if __name__ == "__main__":
    main()
