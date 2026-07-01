import sys
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc
r = run_cbmc("PatchDistanceCodesForBuggyDecoders", "zopfli.c")
print("VERIFIED:", r.is_function_verified)
print(r.response[:3000])
