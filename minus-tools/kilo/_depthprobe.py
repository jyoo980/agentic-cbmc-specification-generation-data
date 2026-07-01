import sys, subprocess, tempfile, shutil, os
sys.path.insert(0,"/app")
from pathlib import Path

SRC="/app/kilo/kilo.c"; FN="editorRefreshScreen"
base=Path(SRC).read_text().split("\n")

# pick a mutant that is clearly OOB: abAppend(&ab,c+j,1) -> c-j  (read before render)
# find the line
import re
target_idx=None
for i,l in enumerate(base):
    if "abAppend(&ab,c+j,1);" in l:
        target_idx=i; break
print("mutating line", target_idx+1, repr(base[target_idx]))

def run(depth, mutate):
    d=tempfile.mkdtemp()
    try:
        L=list(base)
        if mutate:
            L[target_idx]=L[target_idx].replace("c+j","c-j")
        f=os.path.join(d,"m.c"); open(f,"w").write("\n".join(L))
        g=os.path.join(d,"m.goto"); c=os.path.join(d,"chk.goto")
        def sh(cmd): return subprocess.run(cmd,cwd=d,capture_output=True,text=True,timeout=300)
        r=sh(["goto-cc","-o",g,f,"--function",FN])
        if r.returncode!=0: return "GOTOCC:"+r.stderr[-200:]
        sh(["goto-instrument","--partial-loops","--unwind","5",g,g])
        sh(["goto-instrument","--replace-call-with-contract","abAppend",
            "--replace-call-with-contract","abFree",
            "--replace-call-with-contract","editorSyntaxToColor",
            "--enforce-contract",FN,g,c])
        r=sh(["cbmc",c,"--function",FN,"--depth",str(depth)])
        if "VERIFICATION SUCCESSFUL" in r.stdout: return "PASS"
        if "VERIFICATION FAILED" in r.stdout:
            fails=[l for l in r.stdout.splitlines() if ": FAILURE" in l]
            return "FAIL("+str(len(fails))+"): "+(fails[0][:70] if fails else "")
        return "OTHER:"+r.stdout[-150:]
    finally:
        shutil.rmtree(d,ignore_errors=True)

for depth in [200,300,400,600]:
    print(f"depth={depth}: baseline={run(depth,False)} | mutant(c-j)={run(depth,True)}")
