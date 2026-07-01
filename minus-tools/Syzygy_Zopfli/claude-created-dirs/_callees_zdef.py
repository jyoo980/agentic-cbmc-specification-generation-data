from tools.run_cbmc import get_in_file_callees_for, construct_call_graph, CallGraph, get_stub_paths_for, build_stub_index, get_unstubbed_external_callees_for
import json
from pathlib import Path
f='/app/Syzygy_Zopfli/c_code/zopfli.c'
p=construct_call_graph(f)
cg=CallGraph(json.loads(Path(p).read_text()))
print('callees:', get_in_file_callees_for('ZopfliDeflate', cg))
si=build_stub_index()
print('stub_paths:', get_stub_paths_for('ZopfliDeflate', cg, si))
print('nondet:', get_unstubbed_external_callees_for('ZopfliDeflate', cg, si))
print('self-recursive:', cg.is_self_recursive('ZopfliDeflate'))
