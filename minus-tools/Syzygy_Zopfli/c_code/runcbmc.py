"""Mirror the avocado run_cbmc pipeline for one function (helper for local verification)."""
import sys, os
# Drop this script's own directory (c_code) from sys.path so local modules like
# bisect.py do not shadow the stdlib; then make /app importable.
_here = os.path.dirname(os.path.abspath(__file__))
sys.path[:] = [p for p in sys.path if os.path.abspath(p or os.getcwd()) != _here]
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

func = sys.argv[1]
file = sys.argv[2] if len(sys.argv) > 2 else "zopfli.c"
res = run_cbmc(func, file, cwd=_here)
print("=== VERDICT:", str(res), "verified=", res.is_function_verified, "rc=", res.returncode)
print(res.response[-4000:])
