#!/usr/bin/env python3
"""Apply each BoundaryPM mutant (by line number), run the recursion-aware CBMC
pipeline, and report killed/survived. Mirrors tools/run_cbmc.py's retry path:
InitNode and BoundaryPM contracts are substituted via --replace-call-with-contract."""
import subprocess, os, re, sys

SRC = "zopfli.c"
FUNC = "BoundaryPM"
REPLACE = ["InitNode", "BoundaryPM"]


def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FUNC, "--file", SRC],
        capture_output=True, text=True,
    ).stdout
    mutants = []
    # blocks separated by '--- original'
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


def run_pipeline(src):
    g = FUNC + ".goto"
    c = "checking-%s-contracts.goto" % FUNC
    replace_flags = []
    for r in REPLACE:
        replace_flags += ["--replace-call-with-contract", r]
    for cmd in (
        ["goto-cc", "-o", g, src, "--function", FUNC],
        ["goto-instrument", "--partial-loops", "--unwind", "5", g, g],
        ["goto-instrument", *replace_flags, "--enforce-contract", FUNC, g, c],
        ["cbmc", c, "--function", FUNC, "--depth", "200"],
    ):
        r = subprocess.run(cmd, capture_output=True, text=True)
        if cmd[0] == "cbmc":
            return r.returncode, r.stdout
        if r.returncode != 0:
            return -1, r.stdout + r.stderr
    return None, ""


orig_lines = open(SRC).read().split("\n")
mutants = get_mutants()
killed = survived = buildfail = 0
print("Found %d mutants\n" % len(mutants))
for i, (lineno, o, m) in enumerate(mutants):
    cur = orig_lines[lineno - 1]
    tag = "L%d#%d" % (lineno, i)
    if cur.strip() != o.strip():
        print("%-12s SKIP (line mismatch: %r vs %r)" % (tag, cur.strip(), o.strip()))
        continue
    lines = list(orig_lines)
    lines[lineno - 1] = m
    tmp = "mutant_%s.c" % tag.replace("#", "_")
    open(tmp, "w").write("\n".join(lines))
    rc, out = run_pipeline(tmp)
    if rc == -1:
        verdict = "BUILD-FAIL (excluded)"; buildfail += 1
    elif rc == 0:
        verdict = "SURVIVED"; survived += 1
    else:
        verdict = "KILLED"; killed += 1
    print("%-12s %-22s | %s -> %s" % (tag, verdict, o.strip(), m.strip()))
    os.remove(tmp)

total = killed + survived
print("\nKILLED=%d SURVIVED=%d BUILD-FAIL=%d  kill score = %d/%d" %
      (killed, survived, buildfail, killed, total))
