#!/usr/bin/env python3
"""Mutation-testing harness for drawBoard in 2048.c.

Pulls mutants from `get-mutants`, applies each one to a copy of the
file, runs the same CBMC pipeline the grader uses (callee contracts replaced,
drawBoard's contract enforced), and reports KILLED / SURVIVED per mutant.
"""
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

FUNCTION = "drawBoard"
FILE = Path("2048.c").resolve()
CALLEES = ["getColors", "getDigitCount"]


def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FUNCTION, "--file", str(FILE)],
        capture_output=True, text=True, check=True).stdout
    # Each mutant block: a hunk header "@@ -L +L @@" then a '-' line then a '+' line.
    mutants = []
    for m in re.finditer(r"@@ -(\d+) \+\d+ @@\n-(.*)\n\+(.*)", out):
        mutants.append((int(m.group(1)), m.group(2), m.group(3)))
    return mutants


def run_pipeline(src: Path, work: Path) -> bool:
    """Return True iff CBMC reports VERIFICATION SUCCESSFUL (mutant SURVIVED)."""
    goto = work / "m.goto"
    check = work / "check.goto"
    steps = [
        ["goto-cc", "-o", str(goto), str(src), "--function", FUNCTION],
        ["goto-instrument", "--partial-loops", "--unwind", "5", str(goto), str(goto)],
        ["goto-instrument",
         *sum([["--replace-call-with-contract", c] for c in CALLEES], []),
         "--enforce-contract", FUNCTION, str(goto), str(check)],
    ]
    for cmd in steps:
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            return False  # compile/instrument failure counts as KILLED
    r = subprocess.run(["cbmc", str(check), "--function", FUNCTION, "--depth", "200"],
                       capture_output=True, text=True)
    return "VERIFICATION SUCCESSFUL" in r.stdout


def main():
    base = FILE.read_text().splitlines(keepends=True)
    mutants = get_mutants()
    killed = survived = 0
    survivors = []
    with tempfile.TemporaryDirectory() as wd:
        work = Path(wd)
        for i, (ln, old, new) in enumerate(mutants):
            if base[ln - 1].rstrip("\n") != old:
                print(f"[{i}] line {ln} mismatch:\n  file: {base[ln-1]!r}\n  mut : {old!r}")
                sys.exit(2)
            mutated = base.copy()
            mutated[ln - 1] = new + "\n"
            src = work / "m.c"
            src.write_text("".join(mutated))
            survived_flag = run_pipeline(src, work)
            tag = "SURVIVED" if survived_flag else "KILLED"
            if survived_flag:
                survived += 1
                survivors.append((ln, old, new))
            else:
                killed += 1
            print(f"[{i:2}] L{ln}: {tag}   {old.strip()}  ->  {new.strip()}")
    total = killed + survived
    print(f"\nKILLED {killed}/{total}  (score {killed/total:.0%})")
    if survivors:
        print("Survivors:")
        for ln, old, new in survivors:
            print(f"  L{ln}: {old.strip()}  ->  {new.strip()}")


if __name__ == "__main__":
    main()
