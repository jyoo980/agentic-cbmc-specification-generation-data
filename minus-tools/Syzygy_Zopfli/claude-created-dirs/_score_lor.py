from tools.run_cbmc import run_cbmc
import inspect
src = inspect.getsource(run_cbmc)
print("=== run_cbmc source (first 120 lines) ===")
print("\n".join(src.splitlines()[:120]))
print("=== RUNNING ===")
r = run_cbmc('LZ77OptimalRun', '/app/Syzygy_Zopfli/c_code/zopfli.c', include_dirs=['/app/Syzygy_Zopfli/c_code'])
print('verdict:', str(r))
print('verified:', r.is_function_verified)
print('returncode:', r.returncode)
print('failed_step:', r.failed_step)
print('timed_out:', r.timed_out)
