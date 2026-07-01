import sys, tempfile, os
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc
KILO = "/app/kilo/kilo.c"
base = open(KILO).read()

# Replace getWindowSize is_fresh requires with w_ok, and add ret==0 / rows>=0 ensures
v = base
for a, b in [
    ("__CPROVER_requires(__CPROVER_is_fresh(rows, sizeof(*rows)))",
     "__CPROVER_requires(__CPROVER_w_ok(rows, sizeof(*rows)))"),
    ("__CPROVER_requires(__CPROVER_is_fresh(cols, sizeof(*cols)))",
     "__CPROVER_requires(__CPROVER_w_ok(cols, sizeof(*cols)))"),
]:
    assert v.count(a) >= 1, a
    v = v.replace(a, b)
GWS_OLD = "    __CPROVER_ensures(__CPROVER_return_value == 0 ==> *cols != 0)\n{"
GWS_NEW = ("    __CPROVER_ensures(__CPROVER_return_value == 0 ==> *cols != 0)\n"
           "    __CPROVER_ensures(__CPROVER_return_value == 0)\n"
           "    __CPROVER_ensures(__CPROVER_return_value == 0 ==> *rows >= 0)\n{")
assert v.count(GWS_OLD) == 1
v = v.replace(GWS_OLD, GWS_NEW)

def run(txt, fn):
    w = tempfile.mkdtemp()
    p = os.path.join(w, "kilo.c")
    open(p, "w").write(txt)
    return run_cbmc(fn, p, cwd=w)

r = run(v, "updateWindowSize")
print("=== updateWindowSize (gws w_ok + ret==0 + rows>=0) ===")
print("VERIFIED" if r.is_function_verified else f"FAIL rc={r.returncode}")
if not r.is_function_verified:
    for line in r.response.split("\n"):
        if "FAILURE" in line or "no body" in line or "not declared" in line:
            print(" ", line)
