#!/usr/bin/env python3
"""Measure editorReadKey's mutation kill score under the canonical CBMC pipeline.

Pipeline mirrors tools/run_cbmc.py:
  goto-cc -o F.goto kilo.c stubs/readkey.c --function F
  goto-instrument --partial-loops --unwind 5 F.goto F.goto
  goto-instrument --enforce-contract F F.goto checking-F-contracts.goto
  cbmc checking-F-contracts.goto --function F --depth 200

A mutant is KILLED iff verification does NOT report VERIFICATION SUCCESSFUL.
Mutants come from `get-mutants`; each is a single-line unified diff that
we apply by exact-matching the '-' line (line numbers are ignored for safety).
"""
import re
import shutil
import subprocess
import sys

FN = "editorReadKey"
FILE = "kilo.c"
STUB = "/app/stubs/readkey.c"
DEPTH = sys.argv[1] if len(sys.argv) > 1 else "200"


def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FN, "--file", FILE],
        capture_output=True, text=True,
    ).stdout
    blocks = re.split(r"(?=^--- original$)", out, flags=re.M)
    muts = []
    for b in blocks:
        minus = re.search(r"^-(?!-- )(.*)$", b, flags=re.M)
        plus = re.search(r"^\+(?!\+\+ )(.*)$", b, flags=re.M)
        if minus and plus:
            muts.append((minus.group(1), plus.group(1)))
    return muts


def run_pipeline():
    cmds = [
        ["goto-cc", "-o", f"{FN}.goto", FILE, STUB, "--function", FN],
        ["goto-instrument", "--partial-loops", "--unwind", "5", f"{FN}.goto", f"{FN}.goto"],
        ["goto-instrument", "--enforce-contract", FN, f"{FN}.goto", f"checking-{FN}-contracts.goto"],
        ["cbmc", f"checking-{FN}-contracts.goto", "--function", FN, "--depth", DEPTH],
    ]
    for c in cmds:
        r = subprocess.run(c, capture_output=True, text=True)
        if c[0] == "cbmc":
            return r.stdout + r.stderr
        if r.returncode != 0:
            return "PIPELINE_FAIL\n" + r.stdout + r.stderr
    return ""


def main():
    muts = get_mutants()
    shutil.copy(FILE, FILE + ".orig")
    orig = open(FILE + ".orig").read()
    killed = survived = 0
    try:
        for i, (m, p) in enumerate(muts, 1):
            if m not in orig:
                print(f"MUTANT {i}: SKIP (no exact match): -{m!r}")
                continue
            if orig.count(m) != 1:
                print(f"MUTANT {i}: SKIP (ambiguous, {orig.count(m)} matches): -{m!r}")
                continue
            open(FILE, "w").write(orig.replace(m, p, 1))
            out = run_pipeline()
            if "VERIFICATION SUCCESSFUL" in out:
                survived += 1
                print(f"MUTANT {i}: SURVIVED   {m.strip()}  ->  {p.strip()}")
            else:
                killed += 1
                tag = "FAILURE" if "VERIFICATION FAILED" in out else "ERR"
                print(f"MUTANT {i}: KILLED({tag}) {m.strip()}  ->  {p.strip()}")
    finally:
        shutil.copy(FILE + ".orig", FILE)
    print(f"=== KILLED {killed} / {killed + survived} ===")


if __name__ == "__main__":
    main()
