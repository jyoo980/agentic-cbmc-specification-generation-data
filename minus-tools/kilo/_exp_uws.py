#!/usr/bin/env python3
import sys, tempfile, os
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc
KILO = "/app/kilo/kilo.c"

base = open(KILO).read()

GWS_OLD = "    __CPROVER_ensures(__CPROVER_return_value == 0 ==> *cols != 0)\n{"
GWS_NEW = ("    __CPROVER_ensures(__CPROVER_return_value == 0 ==> *cols != 0)\n"
           "    __CPROVER_ensures(__CPROVER_return_value == 0)\n"
           "    __CPROVER_ensures(__CPROVER_return_value == 0 ==> *rows >= 0)\n{")
assert base.count(GWS_OLD) == 1

def run(txt, fn):
    w = tempfile.mkdtemp()
    p = os.path.join(w, "kilo.c")
    open(p, "w").write(txt)
    return run_cbmc(fn, p, cwd=w)

# Config 1: getWindowSize with ret==0 + rows>=0 ensures
v1 = base.replace(GWS_OLD, GWS_NEW)
r = run(v1, "updateWindowSize")
print("=== updateWindowSize (gws ret==0,rows>=0) ===")
print("VERIFIED" if r.is_function_verified else f"FAIL rc={r.returncode}")
if not r.is_function_verified:
    for line in r.response.split("\n"):
        if "FAILURE" in line or "no body" in line or "not declared" in line:
            print(" ", line)

# Also confirm getWindowSize itself still verifies with the added ensures
r2 = run(v1, "getWindowSize")
print("=== getWindowSize self (with added ensures) ===")
print("VERIFIED" if r2.is_function_verified else f"FAIL rc={r2.returncode}")
if not r2.is_function_verified:
    for line in r2.response.split("\n"):
        if "FAILURE" in line:
            print(" ", line)
