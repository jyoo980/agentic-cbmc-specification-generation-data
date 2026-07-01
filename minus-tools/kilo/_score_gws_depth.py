#!/usr/bin/env python3
"""Diagnostic: score getWindowSize mutants at a chosen --depth (NOT canonical).

Canonical depth 200 truncates before any return (global-init prologue), so the
official score is structurally 0.  This harness runs the SAME pipeline at a depth
where the ioctl-branch return is reachable but the second getCursorPosition's
precondition is not yet reached, to measure the spec's real discriminating power.
Usage: python3 _score_gws_depth.py <DEPTH>"""
import subprocess, tempfile, os, sys

KILO = "/app/kilo/kilo.c"
STUB = "/app/stubs/ioctl.c"
FN = "getWindowSize"
DEPTH = sys.argv[1] if len(sys.argv) > 1 else "270"

MUTANTS = [
    ("write_eq->ne(469)",
     "if (write(ofd,seq,strlen(seq)) == -1) {",
     "if (write(ofd,seq,strlen(seq)) != -1) {"),
    ("retval_eq->ne(464)",
     "        retval = getCursorPosition(ifd,ofd,rows,cols);\n        if (retval == -1) goto failed;",
     "        retval = getCursorPosition(ifd,ofd,rows,cols);\n        if (retval != -1) goto failed;"),
    ("write12_ne->eq(462)",
     "if (write(ofd,\"\\x1b[999C\\x1b[999B\",12) != 12) goto failed;",
     "if (write(ofd,\"\\x1b[999C\\x1b[999B\",12) == 12) goto failed;"),
    ("retval_eq->ne(459)",
     "        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);\n        if (retval == -1) goto failed;",
     "        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);\n        if (retval != -1) goto failed;"),
    ("wscol_eq->ne(453)",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col != 0) {"),
    ("ioctl_eq->ne(453)",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {",
     "if (ioctl(1, TIOCGWINSZ, &ws) != -1 || ws.ws_col == 0) {"),
    ("or->and(453)",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {",
     "if (ioctl(1, TIOCGWINSZ, &ws) == -1 && ws.ws_col == 0) {"),
]

def run(src_text):
    w = tempfile.mkdtemp()
    src = os.path.join(w, "kilo.c")
    open(src, "w").write(src_text)
    def sh(c): return subprocess.run(c, cwd=w, capture_output=True, text=True)
    g = os.path.join(w, "g.goto"); chk = os.path.join(w, "chk.goto")
    if sh(["goto-cc","-o",g,src,STUB,"--function",FN]).returncode != 0:
        return "BUILD_FAIL"
    sh(["goto-instrument","--partial-loops","--unwind","5",g,g])
    sh(["goto-instrument","--replace-call-with-contract","getCursorPosition",
        "--enforce-contract",FN,g,chk])
    r = sh(["cbmc",chk,"--function",FN,"--depth",DEPTH])
    out = r.stdout + r.stderr
    if "VERIFICATION SUCCESSFUL" in out: return "SURVIVED"
    if "VERIFICATION FAILED" in out: return "KILLED"
    return "OTHER"

base = open(KILO).read()
print(f"depth={DEPTH}  original:", run(base))
killed = 0
for name, old, new in MUTANTS:
    assert old in base and base.count(old) == 1, f"pattern issue: {name}"
    res = run(base.replace(old, new))
    print(f"{name:22s}: {res}")
    if res == "KILLED": killed += 1
print(f"\nKILL SCORE @depth {DEPTH}: {killed}/{len(MUTANTS)}")
