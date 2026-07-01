#!/usr/bin/env python3
# Kill-score harness for ZopfliLengthLimitedCodeLengths.
# Mirrors the avocado pipeline: replace the four contracted internal callees,
# enforce the function's own contract, run cbmc --depth 200.
import subprocess, re, sys

SRC = "zopfli.c"
FUNC = "ZopfliLengthLimitedCodeLengths"
DEPTH = sys.argv[1] if len(sys.argv) > 1 else "200"

orig = open(SRC).read()
txt = subprocess.run(["get-mutants", "--function", FUNC, "--file", SRC],
                     capture_output=True, text=True).stdout
blocks = re.split(r'\n(?=--- original)', txt)
muts = []
for b in blocks:
    m = re.search(r'@@ -(\d+) \+\d+ @@\n-(.*)\n\+(.*)', b)
    if m:
        muts.append((int(m.group(1)), m.group(2), m.group(3)))
print(f"{len(muts)} mutants parsed; depth={DEPTH}")

def run_pipeline():
    steps = [
        ["goto-cc", "-o", "m.goto", SRC, "--function", FUNC],
        ["goto-instrument", "--partial-loops", "--unwind", "5", "m.goto", "m.goto"],
        ["goto-instrument",
         "--replace-call-with-contract", "BoundaryPM",
         "--replace-call-with-contract", "BoundaryPMFinal",
         "--replace-call-with-contract", "ExtractBitLengths",
         "--replace-call-with-contract", "InitLists",
         "--enforce-contract", FUNC, "m.goto", "mc.goto"],
    ]
    for s in steps:
        r = subprocess.run(s, capture_output=True, text=True)
        if r.returncode != 0:
            return "BUILDFAIL", r.stdout + r.stderr
    r = subprocess.run(["cbmc", "mc.goto", "--function", FUNC, "--depth", DEPTH],
                       capture_output=True, text=True)
    return r.stdout + r.stderr, ""

lines = orig.split('\n')
killed = 0
results = []
for i, (ln, old, new) in enumerate(muts):
    idx = ln - 1
    cur = lines[idx]
    if old.strip() not in cur:
        results.append((ln, old.strip(), new.strip(), "LOCATE-FAIL"))
        continue
    newlines = lines[:]
    newlines[idx] = cur.replace(old.strip(), new.strip())
    open(SRC, 'w').write('\n'.join(newlines))
    out, _ = run_pipeline()
    if "VERIFICATION SUCCESSFUL" in out:
        verdict = "SURVIVED"
    elif "VERIFICATION FAILED" in out:
        verdict = "KILLED"; killed += 1
    elif out == "BUILDFAIL":
        verdict = "KILLED(build)"; killed += 1
    else:
        verdict = "ERROR"
    results.append((ln, old.strip(), new.strip(), verdict))
    print(f"[{i:2}] L{ln}: {old.strip()[:45]:45s} -> {new.strip()[:25]:25s} : {verdict}")
    open(SRC, 'w').write(orig)

open(SRC, 'w').write(orig)
print(f"\nKILL SCORE: {killed}/{len(muts)}")
