import json
from pathlib import Path
from tools.run_cbmc import _run_pipeline, get_in_file_callees_for, build_stub_index, get_stub_paths_for
from tools.construct_call_graph import construct_call_graph
from tools.util.callgraph import CallGraph

f = 'ZopfliLZ77OptimalFixed'
src = '/app/Syzygy_Zopfli/c_code/zopfli.c'
cg = CallGraph(json.loads(Path(construct_call_graph(src)).read_text()))
callees = get_in_file_callees_for(f, cg)
print("CALLEES replaced:", callees)
stub_index = build_stub_index()
stub_paths = get_stub_paths_for(f, cg, stub_index)
print("STUB paths:", stub_paths)
recs = []
import os
os.makedirs('/app/_diag_zlof_work', exist_ok=True)
result, out, err = _run_pipeline(f, callees, src, stub_paths=stub_paths,
    include_dirs=['/app/Syzygy_Zopfli/c_code'], prevent_macro_expansion=False,
    step_records=recs, cwd='/app/_diag_zlof_work')
print("=== STEP RECORDS ===")
for r in recs:
    print(r.get('step'), r.get('returncode'))
print("=== STDOUT tail ===")
print("\n".join(out.splitlines()[-40:]))
