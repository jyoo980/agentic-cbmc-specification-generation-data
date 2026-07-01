import json
from pathlib import Path
from tools.construct_call_graph import construct_call_graph
from tools.util import build_stub_index, get_stub_paths_for, get_in_file_callees_for, get_unstubbed_external_callees_for
from tools.util.callgraph import CallGraph
p = construct_call_graph('/app/kilo/kilo.c')
cg = CallGraph(json.loads(Path(p).read_text()))
fn='editorFind'
print('in-file callees:', get_in_file_callees_for(fn,cg))
si=build_stub_index()
print('stub paths:', get_stub_paths_for(fn,cg,si))
print('nondet external:', get_unstubbed_external_callees_for(fn,cg,si))
print('internal:', cg.get_callees(fn).internal)
print('external:', cg.get_callees(fn).external)
