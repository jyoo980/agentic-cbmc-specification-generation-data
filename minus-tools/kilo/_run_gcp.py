"""Run canonical CBMC verification for getCursorPosition (helper for spec dev)."""
import sys, tempfile
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

fn = sys.argv[1] if len(sys.argv) > 1 else "getCursorPosition"
src = sys.argv[2] if len(sys.argv) > 2 else "/app/kilo/kilo.c"
w = tempfile.mkdtemp()
r = run_cbmc(fn, src, cwd=w)
print("VERIFIED" if r.is_function_verified else "NOT_VERIFIED", "rc=", r.returncode)
print(r.response[:3000])
