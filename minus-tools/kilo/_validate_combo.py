import sys, tempfile, os
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc
KILO = "/app/kilo/kilo.c"
base = open(KILO).read()

# ---- Build candidate: getWindowSize is_fresh->w_ok, +ensures(ret==0), +rows>=0 ----
v = base
# w_ok only on getWindowSize's two requires (line ~441/442). getCursorPosition
# (line ~406/407) also has is_fresh; replacing it too is harmless (not a callee of
# updateWindowSize) but to be safe we leave getCursorPosition's untouched by anchoring
# on getWindowSize's unique surrounding text.
gws_block_old = (
"""    __CPROVER_requires(__CPROVER_is_fresh(rows, sizeof(*rows)))
    __CPROVER_requires(__CPROVER_is_fresh(cols, sizeof(*cols)))
    __CPROVER_assigns(*rows, *cols, rk_idx)
    __CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == -1)""")
gws_block_new = (
"""    __CPROVER_requires(__CPROVER_w_ok(rows, sizeof(*rows)))
    __CPROVER_requires(__CPROVER_w_ok(cols, sizeof(*cols)))
    __CPROVER_assigns(*rows, *cols, rk_idx)
    __CPROVER_ensures(__CPROVER_return_value == 0 || __CPROVER_return_value == -1)
    __CPROVER_ensures(__CPROVER_return_value == 0)""")
assert v.count(gws_block_old) == 1, "gws block not unique"
v = v.replace(gws_block_old, gws_block_new)
ens_old = "    __CPROVER_ensures(__CPROVER_return_value == 0 ==> *cols != 0)\n{"
ens_new = ("    __CPROVER_ensures(__CPROVER_return_value == 0 ==> *cols != 0)\n"
           "    __CPROVER_ensures(__CPROVER_return_value == 0 ==> *rows >= 0)\n{")
assert v.count(ens_old) == 1
v = v.replace(ens_old, ens_new)

def run(txt, fn):
    w = tempfile.mkdtemp()
    p = os.path.join(w, "kilo.c")
    open(p, "w").write(txt)
    return run_cbmc(fn, p, cwd=w)

def verdict(r):
    return "VERIFIED" if r.is_function_verified else f"FAIL rc={r.returncode}"

# 1) updateWindowSize verifies?
print("updateWindowSize:", verdict(run(v, "updateWindowSize")))
# 2) getWindowSize still self-verifies?
print("getWindowSize self:", verdict(run(v, "getWindowSize")))

# 3) getWindowSize 7-mutant score on the MODIFIED contract (must still verify & not regress)
GWS_MUT = [
 ("write_eq->ne", "if (write(ofd,seq,strlen(seq)) == -1) {", "if (write(ofd,seq,strlen(seq)) != -1) {"),
 ("retval2_eq->ne", "        retval = getCursorPosition(ifd,ofd,rows,cols);\n        if (retval == -1) goto failed;",
                     "        retval = getCursorPosition(ifd,ofd,rows,cols);\n        if (retval != -1) goto failed;"),
 ("write12_ne->eq", 'if (write(ofd,"\\x1b[999C\\x1b[999B",12) != 12) goto failed;',
                    'if (write(ofd,"\\x1b[999C\\x1b[999B",12) == 12) goto failed;'),
 ("retval1_eq->ne", "        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);\n        if (retval == -1) goto failed;",
                     "        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);\n        if (retval != -1) goto failed;"),
 ("wscol_eq->ne", "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {",
                  "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col != 0) {"),
 ("ioctl_eq->ne", "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {",
                  "if (ioctl(1, TIOCGWINSZ, &ws) != -1 || ws.ws_col == 0) {"),
 ("or->and", "if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {",
             "if (ioctl(1, TIOCGWINSZ, &ws) == -1 && ws.ws_col == 0) {"),
]
killed = 0
for name, old, new in GWS_MUT:
    assert v.count(old) == 1, name
    r = run(v.replace(old, new), "getWindowSize")
    k = not r.is_function_verified
    killed += k
    print(f"  gws mutant {name:16s}: {'KILLED' if k else 'survived'}")
print(f"getWindowSize kill score (modified): {killed}/7")
