#!/usr/bin/env python3
"""Apply each AddBit mutant, run the CBMC pipeline, report killed/survived."""
import subprocess, sys, os

SRC = "zopfli.c"
FUNC = "AddBit"

MUTANTS = [
    ("1232-if",  "\n    if (*bp == 0)", "\n    if (*bp != 0)"),
    ("1235-dec", "\n    *bp = (*bp + 1) & 7;", "\n    *bp = (*bp - 1) & 7;"),
    ("1234-oob", "\n    (*out)[*outsize - 1] |= bit << *bp;",
                 "\n    (*out)[*outsize + 1] |= bit << *bp;"),
]

def run_pipeline(src):
    g = FUNC + ".goto"
    c = "checking-%s-contracts.goto" % FUNC
    for cmd in (
        ["goto-cc", "-o", g, src, "--function", FUNC],
        ["goto-instrument", "--partial-loops", "--unwind", "5", g, g],
        ["goto-instrument", "--enforce-contract", FUNC, g, c],
        ["cbmc", c, "--function", FUNC, "--depth", "200"],
    ):
        r = subprocess.run(cmd, capture_output=True, text=True)
        if cmd[0] == "cbmc":
            return r.returncode, r.stdout
        if r.returncode != 0:
            return -1, r.stdout + r.stderr
    return None, ""

orig = open(SRC).read()
for name, o, m in MUTANTS:
    assert orig.count(o) == 1, "pattern not unique for %s: %s" % (name, o)
    mutated = orig.replace(o, m)
    tmp = "mutant_%s.c" % name
    open(tmp, "w").write(mutated)
    rc, out = run_pipeline(tmp)
    if rc == -1:
        verdict = "BUILD-FAIL (excluded)"
    elif rc == 0:
        verdict = "SURVIVED (cbmc passed)"
    else:
        verdict = "KILLED (cbmc failed)"
    print("%-10s %s" % (name, verdict))
    os.remove(tmp)
