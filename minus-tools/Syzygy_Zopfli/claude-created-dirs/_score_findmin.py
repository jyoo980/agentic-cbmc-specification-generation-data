from tools.run_cbmc import run_cbmc
r = run_cbmc('FindMinimum', '/app/Syzygy_Zopfli/c_code/zopfli.c', include_dirs=['/app/Syzygy_Zopfli/c_code'])
print('verdict:', str(r))
print('verified:', r.is_function_verified)
print('returncode:', r.returncode)
print('failed_step:', r.failed_step)
for attr in dir(r):
    if not attr.startswith('_'):
        v=getattr(r,attr)
        if not callable(v):
            print('---',attr,'---')
            print(str(v)[:2000])
