#!/usr/bin/env python3
"""Mutation-test harness for editorSave.

For each mutant from get-mutants, apply the single-line change to a
copy of kilo.c, rebuild the canonical goto pipeline (replace editorRowsToString
with its contract, enforce editorSave) and run CBMC at --depth 200.

A mutant is KILLED if CBMC reports VERIFICATION FAILED; SURVIVED if SUCCESSFUL.
"""
import re, shutil, subprocess, sys, os, tempfile

FUNCTION = "editorSave"
SRC = "kilo.c"
DEPTH = "200"

def get_mutants():
    out = subprocess.check_output(
        ["get-mutants", "--function", FUNCTION, "--file", SRC],
        text=True)
    blocks = out.split("--- original")
    mutants = []
    for b in blocks:
        m = re.search(r"@@ -(\d+) \+\d+ @@", b)
        if not m:
            continue
        line = int(m.group(1))
        minus = None
        plus = None
        for ln in b.splitlines():
            if ln.startswith("-    ") or (ln.startswith("-") and not ln.startswith("---")):
                minus = ln[1:]
            elif ln.startswith("+    ") or (ln.startswith("+") and not ln.startswith("+++")):
                plus = ln[1:]
        mutants.append((line, minus, plus))
    return mutants

def run_one(line, minus, plus, idx):
    with open(SRC) as f:
        lines = f.readlines()
    orig = lines[line-1]
    assert orig.rstrip("\n") == minus, f"line {line}: {orig!r} != {minus!r}"
    lines[line-1] = plus + "\n"
    mfile = f"mut_{idx}.c"
    with open(mfile, "w") as f:
        f.writelines(lines)
    goto = f"mut_{idx}.goto"
    chk = f"mut_{idx}_chk.goto"
    def sh(cmd):
        return subprocess.run(cmd, capture_output=True, text=True)
    r = sh(["goto-cc", "-o", goto, mfile, "--function", FUNCTION])
    if r.returncode != 0:
        return ("BUILDFAIL", r.stderr[-500:])
    sh(["goto-instrument", "--partial-loops", "--unwind", "5", goto, goto])
    sh(["goto-instrument", "--replace-call-with-contract", "editorRowsToString",
        "--enforce-contract", FUNCTION, goto, chk])
    r = sh(["cbmc", chk, "--function", FUNCTION, "--depth", DEPTH])
    out = r.stdout + r.stderr
    if "VERIFICATION FAILED" in out:
        return ("KILLED", "")
    if "VERIFICATION SUCCESSFUL" in out:
        return ("SURVIVED", "")
    return ("UNKNOWN", out[-600:])

def main():
    mutants = get_mutants()
    print(f"{len(mutants)} mutants")
    killed = 0
    for i, (line, minus, plus) in enumerate(mutants):
        status, info = run_one(line, minus, plus, i)
        if status == "KILLED":
            killed += 1
        print(f"[{i}] line {line}: {minus.strip()} -> {plus.strip()}  ==> {status}")
        if info:
            print("    ", info.replace("\n", "\n     "))
    print(f"\nKILL SCORE: {killed}/{len(mutants)}")

if __name__ == "__main__":
    main()
