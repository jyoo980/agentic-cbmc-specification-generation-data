import sys, tempfile, shutil
sys.path.insert(0, "/app")
from pathlib import Path
from tools.run_cbmc import run_cbmc

SRC = "/app/kilo/kilo.c"
FN = "editorRowAppendString"
base = Path(SRC).read_text()

def run_on(text, tag):
    d = tempfile.mkdtemp()
    try:
        f = Path(d) / "kilo.c"
        f.write_text(text)
        res = run_cbmc(function_to_verify=FN,
                       file_containing_function_to_verify=str(f), cwd=d)
        print(f"{tag}: VERIFIED={res.is_function_verified} STR={res} rc={res.returncode} failed={res.failed_step}")
        return res
    finally:
        shutil.rmtree(d, ignore_errors=True)

# Probe 1: insert assert(0) at first body statement. SUCCESS => body unreachable (vacuous).
anchor = "{\n    row->chars = realloc(row->chars,row->size+len+1);"
assert base.count(anchor)==1, base.count(anchor)
probe = base.replace(anchor, '{\n    __CPROVER_assert(0, "REACH-body-start");\n    row->chars = realloc(row->chars,row->size+len+1);')
run_on(probe, "P1 assert0-at-body-start (SUCCESS=>unreachable)")
