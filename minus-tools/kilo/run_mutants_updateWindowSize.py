#!/usr/bin/env python3
"""Score the updateWindowSize mutant(s) against its CBMC contract using the
canonical run_cbmc pipeline (--depth 200, replaces getWindowSize with its
contract, auto-links stubs). A mutant is KILLED iff verification fails."""
import sys, tempfile, os
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

KILO = "/app/kilo/kilo.c"
FN = "updateWindowSize"

MUTANTS = [
    ("eq->ne(2333)",
     "                      &E.screenrows,&E.screencols) == -1) {",
     "                      &E.screenrows,&E.screencols) != -1) {"),
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
    print(r0.response[:2500])

killed = 0
for name, old, new in MUTANTS:
    assert old in base, f"pattern not found: {name}"
    assert base.count(old) == 1, f"pattern not unique: {name} ({base.count(old)})"
    r = run(base.replace(old, new))
    res = "KILLED" if not r.is_function_verified else "SURVIVED"
    print(f"{name:18s}: {res} (rc={r.returncode})")
    if res == "KILLED":
        killed += 1
print(f"\nKILL SCORE: {killed}/{len(MUTANTS)}")
