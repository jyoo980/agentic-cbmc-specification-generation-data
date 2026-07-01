import sys, tempfile, shutil, re
sys.path.insert(0, "/app")
from pathlib import Path
from tools.run_cbmc import run_cbmc

SRC = "/app/kilo/kilo.c"
FN = "editorRowAppendString"
base = Path(SRC).read_text()

# locate the function definition block (from signature to its closing brace at col0 before next comment)
sig = "void editorRowAppendString(erow *row, char *s, size_t len)"
start = base.index(sig)
# body end: the line "    E.dirty++;\n}\n"
bodyend = base.index("    E.dirty++;\n}\n", start) + len("    E.dirty++;\n}\n")
func = base[start:bodyend]
prefix = base[:start]
suffix = base[bodyend:]

# add assert0 at body start
func_probe = func.replace("{\n    row->chars = realloc",
                          '{\n    __CPROVER_assert(0,"REACH");\n    row->chars = realloc')

def build(contract_lines, body=func_probe):
    # body already has full contract; we instead rebuild from scratch
    pass

def run_on(text, tag):
    d = tempfile.mkdtemp()
    try:
        f = Path(d) / "kilo.c"
        f.write_text(text)
        res = run_cbmc(function_to_verify=FN, file_containing_function_to_verify=str(f), cwd=d)
        v = res.is_function_verified
        print(f"{tag}: {'UNREACH(vacuous PASS)' if v else 'REACH(FAIL)'}  STR={res} failed={res.failed_step}")
        return res
    finally:
        shutil.rmtree(d, ignore_errors=True)

# Build custom contracts. Body with assert0.
BODY = '{\n    __CPROVER_assert(0,"REACH");\n    row->chars = realloc(row->chars,row->size+len+1);\n    memcpy(row->chars+row->size,s,len);\n    row->size += len;\n    row->chars[row->size] = \'\\0\';\n    editorUpdateRow(row);\n    E.dirty++;\n}\n'

def mk(reqs, assigns, frees="    __CPROVER_frees(row->chars)\n"):
    c = sig + "\n"
    for r in reqs:
        c += f"    __CPROVER_requires({r})\n"
    if assigns:
        c += "    __CPROVER_assigns(" + ", ".join(assigns) + ")\n"
    c += frees
    c += BODY
    return prefix + c + suffix

full_reqs = [
 "__CPROVER_is_fresh(row, sizeof(*row))",
 "row->size == 0", "len == 1",
 "__CPROVER_is_fresh(row->chars, 1)",
 "__CPROVER_is_fresh(s, len)",
 "row->render == NULL",
 "__CPROVER_is_fresh(row->hl, 8)",
 "E.syntax == NULL",
 "0 <= E.dirty && E.dirty < 1000000",
]
full_assigns = ["row->chars","__CPROVER_object_whole(row->chars)","row->size","E.dirty","row->render","row->rsize","row->hl","__CPROVER_object_whole(row->hl)"]

run_on(mk(full_reqs, full_assigns), "V0 full")
# V1: drop object_whole from assigns
run_on(mk(full_reqs, ["row->chars","row->size","E.dirty","row->render","row->rsize","row->hl"]), "V1 no-object_whole")
# V2: empty assigns (just check reachability cost of requires)
run_on(mk(full_reqs, []), "V2 no-assigns")
# V3: minimal requires (only is_fresh row + size/len)
run_on(mk(["__CPROVER_is_fresh(row, sizeof(*row))","row->size == 0","len == 1","__CPROVER_is_fresh(row->chars, 1)","__CPROVER_is_fresh(s, len)"], []), "V3 minimal-req no-assigns")
