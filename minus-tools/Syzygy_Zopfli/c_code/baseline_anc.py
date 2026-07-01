import sys
import os as _os
_os.chdir("/app/Syzygy_Zopfli/c_code")
sys.path[:] = [p for p in sys.path if p not in ("", "/app/Syzygy_Zopfli/c_code", _os.getcwd())]
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

r = run_cbmc("AddNonCompressedBlock", "zopfli.c")
print("is_function_verified =", r.is_function_verified)
