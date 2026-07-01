"""Score getCursorPosition mutants against its CBMC contract (canonical pipeline).

Pulls mutants from get-mutants, applies each by unique whole-line
replacement to kilo.c (keeping the contract), and runs the canonical run_cbmc
(which auto-links stubs/readkey.c via the call graph). A mutant is KILLED iff the
function does not verify.
"""
import re, subprocess, sys, tempfile, os
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

KILO = "/app/kilo/kilo.c"
FN = "getCursorPosition"

def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FN, "--file", KILO],
        capture_output=True, text=True).stdout
    muts = []
    for block in re.split(r"(?m)^--- original$", out):
        if "@@" not in block:
            continue
        old = new = None
        for line in block.splitlines():
            if line.startswith("---") or line.startswith("+++"):
                continue
            if line.startswith("-") and old is None:
                old = line[1:]
            elif line.startswith("+") and new is None:
                new = line[1:]
        if old is not None and new is not None:
            muts.append((old, new))
    return muts

def verify(src_text):
    w = tempfile.mkdtemp()
    src = os.path.join(w, "kilo.c")
    with open(src, "w") as f:
        f.write(src_text)
    r = run_cbmc(FN, src, cwd=w)
    return r

base = open(KILO).read()
r0 = verify(base)
print("original:", "VERIFIED" if r0.is_function_verified else "NOT_VERIFIED", str(r0))

muts = get_mutants()
killed = 0
for i, (old, new) in enumerate(muts):
    if old not in base:
        print(f"[{i}] PATTERN_NOT_FOUND: {old!r}")
        continue
    if base.count(old) != 1:
        print(f"[{i}] AMBIGUOUS({base.count(old)}): {old!r}")
        continue
    mtext = base.replace(old, new)
    r = verify(mtext)
    status = "SURVIVED" if r.is_function_verified else "KILLED"
    if status == "KILLED":
        killed += 1
    print(f"[{i}] {status:8s} [{str(r)}]  {old.strip()}  =>  {new.strip()}")
print(f"\nKILL SCORE: {killed}/{len(muts)}")
