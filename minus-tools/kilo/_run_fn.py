import sys
sys.path.insert(0, "/app")
from tools.run_cbmc import run_cbmc

fn = sys.argv[1]
r = run_cbmc(fn, "/app/kilo/kilo.c")
print("VERDICT:", str(r), "rc:", r.returncode)
if r.returncode != 0:
    print(r.response[:6000])
