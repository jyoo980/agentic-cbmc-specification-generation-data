"""Faithful mutation harness for editorOpen using the REAL canonical run_cbmc
pipeline (same stub auto-selection, --partial-loops --unwind 5, contract
enforcement, cbmc --depth 200).  A mutant is KILLED iff run_cbmc does NOT verify.
"""
import sys, tempfile, shutil
from pathlib import Path
from tools.run_cbmc import run_cbmc

SRC = "/app/kilo/kilo.c"
FN = "editorOpen"

# (label, exact-unique-old-substring, new-substring) -- the 9 avocado mutants.
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

def run_on(src_text):
    d = tempfile.mkdtemp()
    try:
        f = Path(d) / "kilo.c"
        f.write_text(src_text)
        res = run_cbmc(function_to_verify=FN,
                       file_containing_function_to_verify=str(f),
                       cwd=d)
        return res.is_function_verified, str(res)
    finally:
        shutil.rmtree(d, ignore_errors=True)

def main():
    only = sys.argv[1] if len(sys.argv) > 1 else None
    base = Path(SRC).read_text()
    ok, status = run_on(base)
    print(f"### Baseline: {'VERIFIED' if ok else 'FAILED'} ({status})")
    if not ok:
        print("!! baseline does not verify; aborting"); return
    killed = total = 0
    for label, old, new in MUTANTS:
        if only and only not in label:
            continue
        total += 1
        if base.count(old) != 1:
            print(f"{label}: PATCH_ERROR (count={base.count(old)})"); continue
        ok, status = run_on(base.replace(old, new))
        if ok:
            print(f"{label}: SURVIVED")
        else:
            print(f"{label}: KILLED ({status})"); killed += 1
    print(f"\nKILL SCORE: {killed} / {total}")

if __name__ == "__main__":
    main()
