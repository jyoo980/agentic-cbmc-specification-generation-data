import sys
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

r = run_cbmc("editorRowsToString", "/app/kilo/kilo.c")
print("VERDICT:", str(r))
print("returncode:", r.returncode)
print(r.response[:8000])
