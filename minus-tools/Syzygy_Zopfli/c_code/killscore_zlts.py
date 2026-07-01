"""Mutation kill-score for ZopfliLengthsToSymbols using the real run_cbmc pipeline.

Mirrors the avocado ground truth: applies each mutant from `get-mutants`
to the function body, then runs tools.run_cbmc.run_cbmc (which links the /app/stubs
models and uses the same unwind/depth as the harness). A mutant is KILLED iff the
mutated program does not verify.

Run from /app with:
    PYTHONSAFEPATH=1 PYTHONPATH=/app .venv/bin/python \
        /app/Syzygy_Zopfli/c_code/killscore_zlts.py
"""
import re
import subprocess
import sys

from tools.run_cbmc import run_cbmc

SRC = "/app/Syzygy_Zopfli/c_code/zopfli.c"
FUNC = "ZopfliLengthsToSymbols"


def get_mutants():
    out = subprocess.run(
        ["get-mutants", "--function", FUNC, "--file", SRC],
        capture_output=True, text=True,
    ).stdout
    mutants = []
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


def verifies(src_path):
    """True if VERIFIED; None if goto-cc rejects the mutant (excluded)."""
    r = run_cbmc(FUNC, src_path)
    if r.failed_step is not None and r.failed_step.value == "goto-cc":
        return None
    return r.is_function_verified


def main():
    orig_text = open(SRC).read()
    orig_lines = orig_text.split("\n")
    mutants = get_mutants()
    killed = survived = buildfail = 0
    survivors = []
    print("Found %d mutants\n" % len(mutants))
    try:
        for i, (lineno, o, m) in enumerate(mutants):
            cur = orig_lines[lineno - 1]
            tag = "L%d#%d" % (lineno, i)
            if cur.strip() != o.strip():
                print("%-10s SKIP (line mismatch): have %r want %r" % (tag, cur.strip(), o.strip()))
                continue
            lines = list(orig_lines)
            lines[lineno - 1] = m
            open(SRC, "w").write("\n".join(lines))
            res = verifies(SRC)
            if res is None:
                print("%-10s BUILD-FAIL (excluded) | %s" % (tag, m.strip()))
                buildfail += 1
            elif res:
                print("%-10s SURVIVED | %s -> %s" % (tag, o.strip(), m.strip()))
                survived += 1
                survivors.append((tag, o.strip(), m.strip()))
            else:
                print("%-10s KILLED   | %s -> %s" % (tag, o.strip(), m.strip()))
                killed += 1
    finally:
        open(SRC, "w").write(orig_text)

    total = killed + survived
    print("\nKILLED=%d SURVIVED=%d BUILD-FAIL=%d  kill score = %d/%d"
          % (killed, survived, buildfail, killed, total if total else 0))
    if survivors:
        print("\nSurvivors:")
        for tag, o, m in survivors:
            print("  %-10s %s -> %s" % (tag, o, m))


if __name__ == "__main__":
    main()
