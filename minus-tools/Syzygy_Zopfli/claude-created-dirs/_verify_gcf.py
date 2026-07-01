import sys; sys.path.insert(0,'/app')
from tools.run_cbmc import run_cbmc
r = run_cbmc('GetCostFixed', '/app/Syzygy_Zopfli/c_code/zopfli.c')
print('verified:', r.is_function_verified)
