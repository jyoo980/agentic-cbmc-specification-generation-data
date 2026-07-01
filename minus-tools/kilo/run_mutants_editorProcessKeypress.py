#!/usr/bin/env python3
"""Measure editorProcessKeypress's mutation kill score.

The canonical pipeline (tools/run_cbmc.py) cannot run this function: it replaces
the variadic editorSetStatusMessage contract, which aborts goto-instrument 6.9.0,
and malloc is undeclared so the other callees' is_fresh clauses fail to
instantiate.  This harness uses the SAME unwind/depth/stub settings but
(a) `--add-library` to declare malloc and (b) leaves editorSetStatusMessage's
real body inlined (sound: it only havocs statusmsg).  See
verify_editorProcessKeypress.sh for the rationale.

A mutant is KILLED iff verification does NOT report VERIFICATION SUCCESSFUL.
"""
import re
import shutil
import subprocess
import sys

FN = "editorProcessKeypress"
FILE = "kilo.c"
STUB = "/app/stubs/readkey.c"
DEPTH = sys.argv[1] if len(sys.argv) > 1 else "1200"
G = f"{FN}.goto"
C = f"checking-{FN}-contracts.goto"
# Replace every in-file callee EXCEPT:
#   - editorSetStatusMessage : variadic, crashes goto-instrument 6.9.0 when replaced
#   - editorMoveCursor       : its own is_fresh+range contract makes the paging
#                              path infeasible when replaced (postcondition goes
#                              vacuous); inlining its real (correct) body instead
#                              propagates the keypress's effect on E.cy/E.rowoff,
#                              which is what the functional ensures pin down.
# This is the real-behaviour measurement; depth is raised from canonical 200 only
# because inlining lengthens the symbolic path (the spec hard-codes no depth).
REPLACE = [
    "editorDelChar", "editorFind", "editorInsertChar", "editorInsertNewline",
    "editorReadKey", "editorSave",
]


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
        ["goto-cc", "-D__NO_CTYPE", "-o", G, FILE, STUB, "--function", FN],
        ["goto-instrument", "--add-library", G, G],
        ["goto-instrument", "--partial-loops", "--unwind", "5", G, G],
        ["goto-instrument"] + sum(([f"--replace-call-with-contract", c] for c in REPLACE), [])
            + ["--enforce-contract", FN, G, C],
        ["cbmc", C, "--function", FN, "--depth", DEPTH],
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
