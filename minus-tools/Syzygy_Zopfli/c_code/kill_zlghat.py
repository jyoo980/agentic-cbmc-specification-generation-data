#!/usr/bin/env python3
# Kill-test harness for ZopfliLZ77GetHistogramAt.
# Parses avocado mutants, applies each by line number to a fresh copy,
# runs the canonical CBMC pipeline (unwind 5, depth 200), reports kill/survive.
import subprocess, re, sys, os

SRC = "/app/Syzygy_Zopfli/c_code/zopfli.c"
FUNCTION = "ZopfliLZ77GetHistogramAt"
WORK = "/app/_kill_zlghat"

raw = subprocess.run(
    ["get-mutants", "--function", FUNCTION, "--file", SRC],
    capture_output=True, text=True).stdout

# Parse blocks
mutants = []
blocks = raw.split("--- original")
for b in blocks:
    m = re.search(r"@@ -(\d+) \+\d+ @@\n-(.*)\n\+(.*)", b)
    if m:
        ln = int(m.group(1)); orig = m.group(2); mut = m.group(3)
        mutants.append((ln, orig, mut))

print(f"Parsed {len(mutants)} mutants")

os.makedirs(WORK, exist_ok=True)
src_lines = open(SRC).read().split("\n")

killed = 0; survived = []
for idx, (ln, orig, mut) in enumerate(mutants):
    lines = list(src_lines)
    # line numbers are 1-based
    cur = lines[ln-1]
    if cur != orig:
        print(f"[{idx}] LINEMISMATCH ln{ln}: file={cur!r} expected={orig!r}")
        continue
    lines[ln-1] = mut
    mfile = os.path.join(WORK, "mutated.c")
    open(mfile, "w").write("\n".join(lines))
    g = os.path.join(WORK, "m.goto"); gc = os.path.join(WORK, "mc.goto")
    for f in (g, gc):
        if os.path.exists(f): os.remove(f)
    cc = subprocess.run(["goto-cc", "-o", g, mfile, "-I", "/app/Syzygy_Zopfli/c_code",
                         "--function", FUNCTION], capture_output=True, text=True)
    if cc.returncode != 0:
        print(f"[{idx}] CCFAIL ln{ln}: {mut.strip()}\n{cc.stderr[:300]}")
        continue
    subprocess.run(["goto-instrument", "--partial-loops", "--unwind", "5", g, g],
                   capture_output=True, text=True)
    subprocess.run(["goto-instrument", "--enforce-contract", FUNCTION, g, gc],
                   capture_output=True, text=True)
    cb = subprocess.run(["cbmc", gc, "--function", FUNCTION, "--depth", "200"],
                        capture_output=True, text=True)
    out = cb.stdout + cb.stderr
    if "VERIFICATION SUCCESSFUL" in out:
        survived.append((idx, ln, mut.strip()))
        print(f"[{idx}] SURVIVED ln{ln}: {mut.strip()}")
    elif "VERIFICATION FAILED" in out:
        killed += 1
        print(f"[{idx}] KILLED   ln{ln}: {mut.strip()}")
    else:
        print(f"[{idx}] ??? ln{ln}: {mut.strip()}\n{out[-300:]}")

print(f"\n=== KILLED {killed}/{len(mutants)} ===")
print("Survivors:")
for s in survived:
    print(f"  ln{s[1]}: {s[2]}")
