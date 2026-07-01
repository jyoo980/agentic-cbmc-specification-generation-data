from tools.run_cbmc import run_cbmc
src = '/app/Syzygy_Zopfli/c_code/zopfli.c'
inc = ['/app/Syzygy_Zopfli/c_code']
for fn in ['LZ77OptimalRun', 'ZopfliLZ77Optimal']:
    r = run_cbmc(fn, src, include_dirs=inc)
    print(f"{fn}: verified={r.is_function_verified} rc={r.returncode} failed_step={r.failed_step}")
