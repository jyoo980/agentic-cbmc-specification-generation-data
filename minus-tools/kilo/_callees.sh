#!/bin/bash
cd /app
python3 -c "
import json
from pathlib import Path
from tools.construct_call_graph import construct_call_graph
from tools.util import get_in_file_callees_for
from tools.util.callgraph import CallGraph
p=construct_call_graph('/app/kilo/kilo.c')
cg=CallGraph(json.loads(Path(p).read_text()))
print('callees:', get_in_file_callees_for('editorInsertNewline', cg))
"
