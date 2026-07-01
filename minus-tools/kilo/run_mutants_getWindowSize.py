#!/usr/bin/env python3
"""Score the getWindowSize mutants against its CBMC contract using the EXACT
canonical run_cbmc pipeline (auto-links /app/stubs/ioctl.c, replaces
getCursorPosition with its contract, --depth 200).

A mutant is KILLED iff verification fails on the mutated source."""
import sys, tempfile, shutil, os
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

KILO = "/app/kilo/kilo.c"
FN = "getWindowSize"

MUTANTS = [
    ("write_eq->ne(458)",
     "if (write(ofd,seq,strlen(seq)) == -1) {",
     "if (write(ofd,seq,strlen(seq)) != -1) {"),
    ("retval_eq->ne(453)",
     "        retval = getCursorPosition(ifd,ofd,rows,cols);\n        if (retval == -1) goto failed;",
     "        retval = getCursorPosition(ifd,ofd,rows,cols);\n        if (retval != -1) goto failed;"),
    ("write12_ne->eq(451)",
     "if (write(ofd,\"\\x1b[999C\\x1b[999B\",12) != 12) goto failed;",
     "if (write(ofd,\"\\x1b[999C\\x1b[999B\",12) == 12) goto failed;"),
    ("retval_eq->ne(448)",
     "        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);\n        if (retval == -1) goto failed;",
     "        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);\n        if (retval != -1) goto failed;"),
    ("wscol_eq->ne(442)",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col != 0) {"),
    ("ioctl_eq->ne(442)",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {",
     "if (ioctl(1, TIOCGWINSZ, &ws) != -1 || ws.ws_col == 0) {"),
    ("or->and(442)",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 && ws.ws_col == 0) {"),
]

def run(src_text):
    w = tempfile.mkdtemp()
    src = os.path.join(w, "kilo.c")
    with open(src, "w") as f:
        f.write(src_text)
    r = run_cbmc(FN, src, cwd=w)
    return r

base = open(KILO).read()
r0 = run(base)
print("original:", "VERIFIED" if r0.is_function_verified else f"NOT_VERIFIED rc={r0.returncode}")

killed = 0
for name, old, new in MUTANTS:
    assert old in base, f"pattern not found: {name}"
    assert base.count(old) == 1, f"pattern not unique: {name} ({base.count(old)})"
    mtext = base.replace(old, new)
    r = run(mtext)
    res = "KILLED" if not r.is_function_verified else "SURVIVED"
    print(f"{name:22s}: {res} (rc={r.returncode})")
    if res == "KILLED":
        killed += 1
print(f"\nKILL SCORE: {killed}/{len(MUTANTS)}")
