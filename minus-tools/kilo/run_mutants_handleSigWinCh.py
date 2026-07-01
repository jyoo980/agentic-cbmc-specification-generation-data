#!/usr/bin/env python3
"""Score handleSigWinCh's 12 mutants against its CBMC contract using the
canonical run_cbmc pipeline (--depth 200). A mutant is KILLED iff the original
verifies and the mutant does not."""
import sys, tempfile, os
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

KILO = "/app/kilo/kilo.c"
FN = "handleSigWinCh"

CY = "    if (E.cy > E.screenrows) E.cy = E.screenrows - 1;"
CX = "    if (E.cx > E.screencols) E.cx = E.screencols - 1;"

MUTANTS = [
    ("cy:>-><",  CY, CY.replace("E.cy > E.screenrows", "E.cy < E.screenrows")),
    ("cy:>-><=", CY, CY.replace("E.cy > E.screenrows", "E.cy <= E.screenrows")),
    ("cy:>->>=", CY, CY.replace("E.cy > E.screenrows", "E.cy >= E.screenrows")),
    ("cy:>->==", CY, CY.replace("E.cy > E.screenrows", "E.cy == E.screenrows")),
    ("cy:>->!=", CY, CY.replace("E.cy > E.screenrows", "E.cy != E.screenrows")),
    ("cy:-1->+1", CY, CY.replace("E.screenrows - 1", "E.screenrows + 1")),
    ("cx:>-><",  CX, CX.replace("E.cx > E.screencols", "E.cx < E.screencols")),
    ("cx:>-><=", CX, CX.replace("E.cx > E.screencols", "E.cx <= E.screencols")),
    ("cx:>->>=", CX, CX.replace("E.cx > E.screencols", "E.cx >= E.screencols")),
    ("cx:>->==", CX, CX.replace("E.cx > E.screencols", "E.cx == E.screencols")),
    ("cx:>->!=", CX, CX.replace("E.cx > E.screencols", "E.cx != E.screencols")),
    ("cx:-1->+1", CX, CX.replace("E.screencols - 1", "E.screencols + 1")),
]

def run(src_text):
    w = tempfile.mkdtemp()
    src = os.path.join(w, "kilo.c")
    with open(src, "w") as f:
        f.write(src_text)
    return run_cbmc(FN, src, cwd=w)

base = open(KILO).read()
r0 = run(base)
print("original:", "VERIFIED" if r0.is_function_verified else f"NOT_VERIFIED rc={r0.returncode}")
if not r0.is_function_verified:
    print(r0.response[:1500]); sys.exit(0)

killed = 0
for name, old, new in MUTANTS:
    assert old in base, f"pattern not found: {name}"
    assert new != old
    r = run(base.replace(old, new))
    res = "KILLED" if not r.is_function_verified else "SURVIVED"
    print(f"{name:12s}: {res} (rc={r.returncode})")
    if res == "KILLED":
        killed += 1
print(f"\nKILL SCORE: {killed}/{len(MUTANTS)}")
