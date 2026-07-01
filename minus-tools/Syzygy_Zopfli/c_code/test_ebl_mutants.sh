#!/bin/sh
# Test each ExtractBitLengths mutant against the current spec.
FUNCTION=ExtractBitLengths
FILE=zopfli.c
get-mutants --function "$FUNCTION" --file "$FILE" > /tmp/ebl_mutants.txt 2>&1
# split mutants by '--- original'
python3 - <<'PY'
import subprocess, re, shutil
src="zopfli.c"
orig=open(src).read()
txt=open("/tmp/ebl_mutants.txt").read()
# parse unified diff blocks
blocks=re.split(r'\n(?=--- original)', txt)
muts=[]
for b in blocks:
    m=re.search(r'@@ -(\d+) \+\d+ @@\n-(.*)\n\+(.*)', b)
    if m:
        line=int(m.group(1)); old=m.group(2); new=m.group(3)
        muts.append((line,old,new))
print(f"{len(muts)} mutants parsed")
lines=orig.split('\n')
killed=0
for i,(ln,old,new) in enumerate(muts):
    idx=ln-1
    cur=lines[idx]
    # apply: replace old-content with new-content. The diff lines are without leading char already (we stripped). compare trimmed
    if old.strip() not in cur:
        print(f"[{i}] WARN cannot locate old at line {ln}: {cur!r} vs {old!r}")
        continue
    newline=cur.replace(old.strip(), new.strip())
    newlines=lines[:]; newlines[idx]=newline
    open(src,'w').write('\n'.join(newlines))
    r=subprocess.run(["./verify.sh","ExtractBitLengths"],capture_output=True,text=True)
    out=r.stdout+r.stderr
    if "VERIFICATION SUCCESSFUL" in out:
        verdict="SURVIVED"
    elif "VERIFICATION FAILED" in out:
        verdict="KILLED"; killed+=1
    else:
        verdict="ERROR"
    print(f"[{i}] L{ln}: {old.strip()}  ->  {new.strip()}  : {verdict}")
    open(src,'w').write(orig)
print(f"KILL SCORE: {killed}/{len(muts)}")
PY
