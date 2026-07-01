"""Mutation-measurement harness for editorOpen.

The canonical pipeline (--depth 200) cannot score editorOpen at all: the
is_fresh precondition harness alone consumes the entire 200-step budget, so not
even the first body statement is reached and every body mutant survives
vacuously.  This harness measures the *real* strength of editorOpen's spec the
same way the editorUpdateRow harness does -- full path exploration (loops bounded
by --partial-loops --unwind 5, no --depth cap) with the safety checks enabled,
the editorOpen-scoped library models in /app/stubs/editoropen.c, and a
nondeterministic editorInsertRow stub linked in place of the contract (whose
E.numrows == 2 precondition is incompatible with the multi-iteration loop).

A mutant is KILLED iff the pipeline does NOT report VERIFICATION SUCCESSFUL.
"""
import subprocess, sys, tempfile, shutil
from pathlib import Path

SRC = "/app/kilo/kilo.c"
STUB_LIB = "/app/stubs/editoropen.c"
STUB_EIR = "/app/kilo/stub_editorInsertRow_eo.c"
FN = "editorOpen"

MUTANTS = [
    ("M1 ==r -> !=r", "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))",
                       "line[linelen-1] == '\\n' || line[linelen-1] != '\\r'))"),
    ("M2 ==n -> !=n", "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))",
                       "line[linelen-1] != '\\n' || line[linelen-1] == '\\r'))"),
    ("M3 !=-1 -> ==-1", "getline(&line,&linecap,fp)) != -1)",
                        "getline(&line,&linecap,fp)) == -1)"),
    ("M4 errno!= -> ==", "if (errno != ENOENT)",
                         "if (errno == ENOENT)"),
    ("M5 [len-1]r -> [len+1]r", "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))",
                                "line[linelen-1] == '\\n' || line[linelen+1] == '\\r'))"),
    ("M6 [len-1]n -> [len+1]n", "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))",
                                "line[linelen+1] == '\\n' || line[linelen-1] == '\\r'))"),
    ("M7 strlen+1 -> -1", "size_t fnlen = strlen(filename)+1;",
                          "size_t fnlen = strlen(filename)-1;"),
    ("M8 && -> ||", "if (linelen && (line",
                    "if (linelen || (line"),
    ("M9 || -> &&", "line[linelen-1] == '\\n' || line[linelen-1] == '\\r'))",
                    "line[linelen-1] == '\\n' && line[linelen-1] == '\\r'))"),
]

CBMC_CHECKS = [
    "--bounds-check", "--pointer-check", "--pointer-primitive-check",
    "--pointer-overflow-check", "--conversion-check",
    "--signed-overflow-check", "--unsigned-overflow-check", "--no-malloc-may-fail",
]

def sh(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, shell=True, capture_output=True, text=True)

# editorOpen's writes to the dynamically-allocated `line` buffer (allocated inside
# the inlined getline) are not nameable in any assigns frame, so --enforce-contract
# rejects them.  None of editorOpen's mutants are killed by the assigns/ensures
# frame anyway -- every kill is inline (bounds checks, the editorInsertRow stub's
# preconditions, the exit() assertion, the memcpy source bound).  So instead of
# enforcing the contract we drive editorOpen from a small harness appended to the
# temp copy (which can see the file-static E), establishing the same preconditions
# the contract states.  The real kilo.c is untouched.
HARNESS = r"""
void _eo_harness(void)
{
    char *fn = malloc(8);
    __CPROVER_assume(fn != (char *)0);
    __CPROVER_assume(fn[7] == '\0');
    E.filename = malloc(1);
    __CPROVER_assume(E.filename != (char *)0);
    editorOpen(fn);
}
"""
HARNESS_FN = "_eo_harness"

def verifies(src_text):
    src_text = src_text + HARNESS
    d = tempfile.mkdtemp()
    try:
        Path(d, "kilo.c").write_text(src_text)
        steps = [
            f"goto-cc -o eo.goto {d}/kilo.c {STUB_LIB} --function {HARNESS_FN}",
            f"goto-instrument --remove-function-body editorInsertRow eo.goto eo.goto",
            f"goto-cc -o eir.goto {STUB_EIR}",
            f"goto-cc -o eo.goto eo.goto eir.goto --function {HARNESS_FN}",
            f"goto-instrument --partial-loops --unwind 5 eo.goto eo.goto",
        ]
        for s in steps:
            r = sh(s, d)
            if r.returncode != 0:
                return None, f"STEP_FAIL: {s}\n{r.stdout}\n{r.stderr}"
        r = sh(f"cbmc eo.goto --function {HARNESS_FN} " + " ".join(CBMC_CHECKS), d)
        ok = "VERIFICATION SUCCESSFUL" in r.stdout
        return ok, r.stdout
    finally:
        shutil.rmtree(d, ignore_errors=True)

def main():
    only = sys.argv[1] if len(sys.argv) > 1 else None
    base = Path(SRC).read_text()
    ok, out = verifies(base)
    print(f"### Baseline: {'VERIFIED' if ok else 'FAILED/' + str(ok)}")
    if ok is not True:
        # show failure detail
        for ln in out.splitlines():
            if "FAILURE" in ln or "STEP_FAIL" in ln or "VERIFICATION" in ln:
                print("   ", ln)
        if not only:
            print("!! baseline must verify; aborting"); return
    killed = total = 0
    for label, old, new in MUTANTS:
        if only and only not in label:
            continue
        total += 1
        if base.count(old) != 1:
            print(f"{label}: PATCH_ERROR (count={base.count(old)})"); continue
        ok, out = verifies(base.replace(old, new))
        if ok is True:
            print(f"{label}: SURVIVED")
        elif ok is None:
            print(f"{label}: BUILD_ERROR\n{out[:500]}")
        else:
            killed += 1
            fl = [l for l in out.splitlines() if "FAILURE" in l]
            print(f"{label}: KILLED  ({fl[0].strip() if fl else 'verification failed'})")
    print(f"\nKILL SCORE: {killed} / {total}")

if __name__ == "__main__":
    main()
