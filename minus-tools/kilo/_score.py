import sys, re, subprocess, tempfile, shutil, concurrent.futures
sys.path.insert(0, "/app")
from pathlib import Path
from tools.run_cbmc import run_cbmc
from tools.util.callgraph import CallGraph
from tools.construct_call_graph import construct_call_graph
import json

SRC = "/app/kilo/kilo.c"
FN = "editorRefreshScreen"
base = Path(SRC).read_text()
lines = base.split("\n")

# Get mutants
out = subprocess.run(["get-mutants","--function",FN,"--file",SRC],
                     capture_output=True, text=True).stdout
muts = []
for b in out.split("--- original")[1:]:
    m = re.search(r'@@ -(\d+)', b)
    if not m: continue
    ln = int(m.group(1))
    plus = None
    for x in b.splitlines():
        if x.startswith('+++'): continue
        if x.startswith('+'): plus = x[1:]; break
    if plus is not None:
        muts.append((ln, plus))
print(f"{len(muts)} mutants")

cg = CallGraph(json.loads(Path(construct_call_graph(SRC)).read_text()))

def classify(arg):
    ln, plus = arg
    d = tempfile.mkdtemp()
    try:
        L = list(lines); L[ln-1] = plus
        f = Path(d)/"kilo.c"; f.write_text("\n".join(L))
        r = run_cbmc(FN, str(f), call_graph=cg, cwd=d)
        return (ln, plus, r.is_function_verified, str(r))
    finally:
        shutil.rmtree(d, ignore_errors=True)

killed=survived=other=0; surv=[]; oth=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as ex:
    for ln, plus, verified, st in ex.map(classify, muts):
        if st in ("CBMC_FAILED",) and not verified:
            killed+=1
        elif verified:
            survived+=1; surv.append((ln,plus.strip(),st))
        else:
            other+=1; oth.append((ln,plus.strip(),st))
print(f"KILLED={killed} SURVIVED={survived} OTHER={other} TOTAL={len(muts)}")
print("--- SURVIVORS ---")
for ln,p,st in sorted(surv): print(f"  L{ln}: {p[:75]}")
print("--- OTHER (non-CBMC-fail, non-verify) ---")
for ln,p,st in sorted(oth): print(f"  L{ln} [{st}]: {p[:60]}")
