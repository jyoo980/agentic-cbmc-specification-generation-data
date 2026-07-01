import sys
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc
r = run_cbmc("editorInsertRow", "/app/kilo/kilo.c")
print("VERDICT:", str(r), "rc:", r.returncode)
print(r.response[:1500])
