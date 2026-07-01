import sys
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

r = run_cbmc("BoundaryPM", "/app/Syzygy_Zopfli/c_code/zopfli.c")
print("VERDICT:", str(r))
print("is_function_verified:", r.is_function_verified)
print("returncode:", r.returncode, "failed_step:", r.failed_step, "timed_out:", r.timed_out)
print("----- response tail -----")
print(r.response[-3000:])
