#!/usr/bin/env python3
# Grid-search precondition (n, maxbits) values for ZopfliLengthLimitedCodeLengths
# to maximize the depth-200 kill score.  Reuses the same pipeline as the avocado
# harness.  Usage: python3 zllcl_grid.py "N_EXPR" "MAXBITS_EXPR"
import subprocess, re, sys

SRC = "zopfli.c"
FUNC = "ZopfliLengthLimitedCodeLengths"
DEPTH = "200"

orig = open(SRC).read()
N_REQ = sys.argv[1]            # e.g. "n == 1"
MB_REQ = sys.argv[2]           # e.g. "maxbits == 0"

# Patch the two precondition lines (623,624 in original: n==2, maxbits==1).
patched = orig.replace("__CPROVER_requires(n == 2)", f"__CPROVER_requires({N_REQ})")
patched = patched.replace("__CPROVER_requires(maxbits == 1)", f"__CPROVER_requires({MB_REQ})")
assert patched != orig, "patch did not apply"

txt = subprocess.run(["get-mutants", "--function", FUNC, "--file", SRC],
                     capture_output=True, text=True).stdout
blocks = re.split(r'\n(?=--- original)', txt)
muts = []
for b in blocks:
    m = re.search(r'@@ -(\d+) \+\d+ @@\n-(.*)\n\+(.*)', b)
    if m:
        muts.append((int(m.group(1)), m.group(2), m.group(3)))

def run_pipeline(src_text):
    open(SRC, 'w').write(src_text)
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
            return "BUILDFAIL"
    r = subprocess.run(["cbmc", "mc.goto", "--function", FUNC, "--depth", DEPTH],
                       capture_output=True, text=True)
    return r.stdout + r.stderr

# 1) original spec must verify
base = run_pipeline(patched)
base_ok = "VERIFICATION SUCCESSFUL" in base
print(f"REQ: {N_REQ} ; {MB_REQ}  baseline_verifies={base_ok}")
if not base_ok:
    open(SRC, 'w').write(orig)
    print("  -> baseline does NOT verify; skipping")
    sys.exit(0)

lines = patched.split('\n')
killed = 0
detail = []
for (ln, old, new) in muts:
    idx = ln - 1
    cur = lines[idx]
    if old.strip() not in cur:
        detail.append((ln, "LOCATE-FAIL"))
        continue
    newlines = lines[:]
    newlines[idx] = cur.replace(old.strip(), new.strip())
    out = run_pipeline('\n'.join(newlines))
    if "VERIFICATION SUCCESSFUL" in out:
        v = "SURVIVED"
    elif "VERIFICATION FAILED" in out or out == "BUILDFAIL":
        v = "KILLED"; killed += 1
    else:
        v = "ERROR"
    detail.append((ln, v))

open(SRC, 'w').write(orig)
print(f"KILL SCORE: {killed}/{len(muts)}")
for ln, v in detail:
    if v != "SURVIVED":
        print(f"   L{ln}: {v}")
