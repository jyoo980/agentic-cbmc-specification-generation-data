import sys
from tools.run_cbmc import run_cbmc

fn = sys.argv[1]
r = run_cbmc(fn, '/app/Syzygy_Zopfli/c_code/zopfli.c')
print(fn, '->', str(r), 'rc=', r.returncode)
if r.returncode != 0:
    print(r.response[:6000])
