import sys, os
os.chdir("/app/Syzygy_Zopfli/c_code")
sys.path[:] = [p for p in sys.path if p not in ("", os.getcwd())]
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

SRC = "zopfli.c"
PRISTINE = "/tmp/zpristine_body.c"

def splice(contract):
    t = open(PRISTINE).read()
    open(SRC, "w").write(t.replace("/*CONTRACT_MARKER*/", contract))

def verified(tmpfile):
    return run_cbmc("BoundaryPMFinal", tmpfile).is_function_verified

contract = open(sys.argv[1]).read()
splice(contract)
orig_ok = verified(SRC)
m = open(SRC).read().replace("newchain->count = lastcount + 1;",
                             "newchain->count = lastcount - 1;")
open("mtmp.c", "w").write(m)
mut_ok = verified("mtmp.c")
os.remove("mtmp.c")
status = "GOOD (non-vacuous)" if (orig_ok and not mut_ok) else (
         "VACUOUS" if (orig_ok and mut_ok) else "ORIG-FAILS")
print("ORIG verified =", orig_ok, "| MUT(count-1) verified =", mut_ok, "|", status)
