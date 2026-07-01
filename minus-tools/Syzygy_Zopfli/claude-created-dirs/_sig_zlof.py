import inspect
from tools import run_cbmc as m
print("DEPTH", m._CBMC_DEPTH, "UNWIND", m._CBMC_UNWIND)
print(inspect.getsource(m._get_goto_cc_command))
