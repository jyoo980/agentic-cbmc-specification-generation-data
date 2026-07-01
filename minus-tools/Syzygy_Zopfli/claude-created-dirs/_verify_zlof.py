from tools.run_cbmc import run_cbmc

r = run_cbmc('ZopfliLZ77OptimalFixed', '/app/Syzygy_Zopfli/c_code/zopfli.c',
             include_dirs=['/app/Syzygy_Zopfli/c_code'])
print('verdict:', str(r))
print('verified:', r.is_function_verified)
print('returncode:', r.returncode)
print('failed_step:', r.failed_step)
print('timed_out:', r.timed_out)
