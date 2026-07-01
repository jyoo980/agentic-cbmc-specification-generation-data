import sys, json
sys.path.insert(0, "/app")
from pathlib import Path
from tools.construct_call_graph import construct_call_graph
from tools.util import (build_stub_index, get_stub_paths_for,
                        get_in_file_callees_for, get_unstubbed_external_callees_for)
from tools.util.callgraph import CallGraph
p = construct_call_graph("/app/kilo/kilo.c")
cg = CallGraph(json.loads(Path(p).read_text()))
si = build_stub_index()
for fn in ["updateWindowSize", "getWindowSize"]:
    print(fn, "in-file callees:", get_in_file_callees_for(fn, cg))
    print(fn, "stub paths:", get_stub_paths_for(fn, cg, si))
    print(fn, "unstubbed ext:", get_unstubbed_external_callees_for(fn, cg, si))
    print()
