#!/usr/bin/env python3
"""Strength (kill-score) harness for editorRowsToString.

The canonical run_cbmc pipeline cannot verify editorRowsToString: its only
externals are malloc/memcpy, and (a) with default malloc-may-fail the un-guarded
malloc result is a real NULL-deref CBMC reports, while (b) --add-library makes
goto-instrument crash on the builtin malloc model's bool `should_malloc_fail`.

This harness instead uses the SAME structure as verify_editorInsertRow.sh: the
builtin malloc (recognised by NAME at enforce so the returned buffer's writes are
in-frame, no body at enforce so no should_malloc_fail) plus
`cbmc --no-malloc-may-fail`.  A mutant is KILLED iff verification fails.
"""
import os, re, subprocess, tempfile, shutil, concurrent.futures

KILO = "/app/kilo/kilo.c"
FN = "editorRowsToString"

# (label, lineno, old-exact, new) -- avocado's 11 mutants, by current line number.
MUTANTS = [
    ("cmp987 < -> <=", 987, "for (j = 0; j < E.numrows; j++)", "for (j = 0; j <= E.numrows; j++)"),
    ("cmp987 < -> >",  987, "for (j = 0; j < E.numrows; j++)", "for (j = 0; j > E.numrows; j++)"),
    ("cmp987 < -> >=", 987, "for (j = 0; j < E.numrows; j++)", "for (j = 0; j >= E.numrows; j++)"),
    ("cmp987 < -> ==", 987, "for (j = 0; j < E.numrows; j++)", "for (j = 0; j == E.numrows; j++)"),
    ("cmp987 < -> !=", 987, "for (j = 0; j < E.numrows; j++)", "for (j = 0; j != E.numrows; j++)"),
    ("cmp993 < -> <=", 993, "for (j = 0; j < E.numrows; j++) {", "for (j = 0; j <= E.numrows; j++) {"),
    ("cmp993 < -> >",  993, "for (j = 0; j < E.numrows; j++) {", "for (j = 0; j > E.numrows; j++) {"),
    ("cmp993 < -> >=", 993, "for (j = 0; j < E.numrows; j++) {", "for (j = 0; j >= E.numrows; j++) {"),
    ("cmp993 < -> ==", 993, "for (j = 0; j < E.numrows; j++) {", "for (j = 0; j == E.numrows; j++) {"),
    ("cmp993 < -> !=", 993, "for (j = 0; j < E.numrows; j++) {", "for (j = 0; j != E.numrows; j++) {"),
    ("totlen +1 -> -1", 988, "totlen += E.row[j].size+1;", "totlen += E.row[j].size-1;"),
]

def build(lineno, old, new):
    lines = open(KILO).read().split('\n')
    if lineno is not None:
        assert old in lines[lineno-1], f"L{lineno}: {old!r} not in {lines[lineno-1]!r}"
        lines[lineno-1] = lines[lineno-1].replace(old, new)
    return '\n'.join(lines)

def run(src):
    d = tempfile.mkdtemp()
    try:
        f = os.path.join(d, "k.c")
        open(f, 'w').write(src)
        g = os.path.join(d, "m.goto"); c = os.path.join(d, "chk.goto")
        def sh(cmd, t=120): return subprocess.run(cmd, cwd=d, capture_output=True, text=True, timeout=t)
        r = sh(["goto-cc", "-o", g, f, "--function", FN])
        if r.returncode: return ("GOTOCC_ERR", r.stderr[-300:])
        sh(["goto-instrument", "--partial-loops", "--unwind", "5", g, g])
        r = sh(["goto-instrument", "--enforce-contract", FN, g, c])
        if r.returncode: return ("ENFORCE_ERR", r.stderr[-300:])
        try:
            r = sh(["cbmc", c, "--function", FN, "--no-malloc-may-fail"], t=300)
        except subprocess.TimeoutExpired:
            return ("TIMEOUT", "")
        if "VERIFICATION SUCCESSFUL" in r.stdout: return ("SUCCESS", "")
        if "VERIFICATION FAILED" in r.stdout:
            return ("FAILED", [l for l in r.stdout.splitlines() if ": FAILURE" in l][:3])
        return ("OTHER", r.stdout[-300:])
    finally:
        shutil.rmtree(d, ignore_errors=True)

def classify(m):
    label, lineno, old, new = m
    v, info = run(build(lineno, old, new))
    return (label, v, info)

def main():
    bv, bi = run(open(KILO).read())
    print(f"BASELINE: {bv} {bi if bv!='SUCCESS' else ''}")
    if bv != "SUCCESS":
        print("baseline does not verify; aborting"); return
    killed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as ex:
        for label, v, info in ex.map(classify, MUTANTS):
            tag = "KILL" if v == "FAILED" else ("SURV" if v == "SUCCESS" else v)
            if v == "FAILED": killed += 1
            print(f"  [{tag}] {label}  {info if v not in ('SUCCESS','FAILED') else ''}")
    print(f"\nKILL SCORE: {killed} / {len(MUTANTS)}")

if __name__ == "__main__":
    main()
