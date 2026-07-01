import sys, tempfile, shutil
sys.path.insert(0, "/app")
from pathlib import Path
from tools.run_cbmc import run_cbmc

SRC = "/app/kilo/kilo.c"
FN = "editorRowAppendString"

# The 3 avocado mutants (exact unique old-substring -> new-substring).
MUTANTS = [
    ("M1 memcpy +size -> -size",
     "memcpy(row->chars+row->size,s,len);",
     "memcpy(row->chars-row->size,s,len);"),
    ("M2 realloc +len+1 -> +len-1",
     "realloc(row->chars,row->size+len+1);",
     "realloc(row->chars,row->size+len-1);"),
    ("M3 realloc +len+1 -> -len+1",
     "realloc(row->chars,row->size+len+1);",
     "realloc(row->chars,row->size-len+1);"),
]

def run_on(text):
    d = tempfile.mkdtemp()
    try:
        f = Path(d) / "kilo.c"
        f.write_text(text)
        res = run_cbmc(function_to_verify=FN,
                       file_containing_function_to_verify=str(f), cwd=d)
        return res.is_function_verified, str(res)
    finally:
        shutil.rmtree(d, ignore_errors=True)

base = Path(SRC).read_text()
ok, status = run_on(base)
print(f"### Baseline: {'VERIFIED' if ok else 'FAILED'} ({status})")
killed = 0
for label, old, new in MUTANTS:
    assert base.count(old) == 1, f"{label}: old not unique ({base.count(old)})"
    ok, status = run_on(base.replace(old, new))
    verdict = "KILLED" if not ok else "SURVIVED"
    if not ok:
        killed += 1
    print(f"{label}: {verdict} ({status})")
print(f"### Kill score: {killed}/{len(MUTANTS)}")
