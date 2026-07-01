"""Ground-truth verification helper for ZopfliLengthsToSymbols.

Invokes tools.run_cbmc.run_cbmc exactly like verify does, so the verdict
matches the harness (including the stub index and the missing-body --add-library
retry). Run from /app so the local bisect.py does not shadow stdlib bisect.

Usage:  /app/.venv/bin/python /app/Syzygy_Zopfli/c_code/gt_verify.py [FUNCTION] [FILE]
"""
import sys

from tools.run_cbmc import run_cbmc

func = sys.argv[1] if len(sys.argv) > 1 else "ZopfliLengthsToSymbols"
file = sys.argv[2] if len(sys.argv) > 2 else "/app/Syzygy_Zopfli/c_code/zopfli.c"

r = run_cbmc(func, file)
print("VERDICT:", r)
print("is_verified:", r.is_function_verified)
if not r.is_function_verified:
    print(r.response[:4000])
