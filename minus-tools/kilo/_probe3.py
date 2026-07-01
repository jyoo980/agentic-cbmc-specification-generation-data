import sys, tempfile, shutil
sys.path.insert(0, "/app")
from pathlib import Path
from tools.run_cbmc import run_cbmc

SRC = "/app/kilo/kilo.c"
FN = "editorRowAppendString"
base = Path(SRC).read_text()
sig = "void editorRowAppendString(erow *row, char *s, size_t len)"
start = base.index(sig)
bodyend = base.index("    E.dirty++;\n}\n", start) + len("    E.dirty++;\n}\n")
prefix, suffix = base[:start], base[bodyend:]

BODY = ('{\n    __CPROVER_assert(0,"REACH");\n'
        '    row->chars = realloc(row->chars,row->size+len+1);\n'
        '    memcpy(row->chars+row->size,s,len);\n'
        '    row->size += len;\n'
        "    row->chars[row->size] = '\\0';\n"
        '    editorUpdateRow(row);\n    E.dirty++;\n}\n')
ASSIGNS = ["row->chars","__CPROVER_object_whole(row->chars)","row->size","E.dirty","row->render","row->rsize","row->hl","__CPROVER_object_whole(row->hl)"]

def mk(reqs):
    c = sig + "\n"
    for r in reqs: c += f"    __CPROVER_requires({r})\n"
    c += "    __CPROVER_assigns(" + ", ".join(ASSIGNS) + ")\n    __CPROVER_frees(row->chars)\n" + BODY
    return prefix + c + suffix

def run_on(reqs, tag):
    d = tempfile.mkdtemp()
    try:
        f = Path(d)/"kilo.c"; f.write_text(mk(reqs))
        res = run_cbmc(function_to_verify=FN, file_containing_function_to_verify=str(f), cwd=d)
        v = res.is_function_verified
        print(f"{tag}: {'UNREACH' if v else 'REACH-or-fail'} STR={res} failed={res.failed_step}")
    finally:
        shutil.rmtree(d, ignore_errors=True)

R = {
 'frow':"__CPROVER_is_fresh(row, sizeof(*row))",
 's0':"row->size == 0", 'l1':"len == 1",
 'fchars':"__CPROVER_is_fresh(row->chars, 1)",
 'fs':"__CPROVER_is_fresh(s, len)",
 'rn':"row->render == NULL",
 'fhl':"__CPROVER_is_fresh(row->hl, 8)",
 'esn':"E.syntax == NULL",
 'dr':"0 <= E.dirty && E.dirty < 1000000",
}
full = list(R.values())
run_on(full, "B0 full")
# remove one is_fresh at a time
run_on([R[k] for k in R if k!='fhl'], "minus fhl")
run_on([R[k] for k in R if k!='fs'], "minus fs")
run_on([R[k] for k in R if k!='fchars'], "minus fchars")
run_on([R[k] for k in R if k!='frow'], "minus frow")
