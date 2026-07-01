#!/bin/bash
# Mutation kill-testing harness for OptimizeHuffmanForRle using the documented
# grader pipeline (stub + --partial-loops --unwind 5 + --replace-call-with-contract
# AbsDiff --enforce-contract + --depth 200).
#
# Usage: ./killtest.sh
# Expects: /app/Syzygy_Zopfli/c_code/zopfli.c already holds the spec to test.
set -u
cd /app
FUNCTION=OptimizeHuffmanForRle
SRC=Syzygy_Zopfli/c_code/zopfli.c
WORK=_kill_ohfr
mkdir -p $WORK
STUB=stubs/cprover_alloc.c

build_and_check () {
  local file="$1"
  local tag="$2"
  goto-cc -o $WORK/${tag}.goto "$file" $STUB --function ${FUNCTION} >/dev/null 2>&1 || { echo "GOTOCC_ERR"; return; }
  goto-instrument --partial-loops --unwind 5 $WORK/${tag}.goto $WORK/${tag}.goto >/dev/null 2>&1 || { echo "INSTR1_ERR"; return; }
  goto-instrument --replace-call-with-contract AbsDiff --enforce-contract ${FUNCTION} $WORK/${tag}.goto $WORK/${tag}-c.goto >/dev/null 2>&1 || { echo "INSTR2_ERR"; return; }
  local out
  out=$(cbmc $WORK/${tag}-c.goto --function ${FUNCTION} --depth 200 2>&1)
  if echo "$out" | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "SUCCESS"
  elif echo "$out" | grep -q "VERIFICATION FAILED"; then
    echo "FAILED"
  else
    echo "OTHER"
  fi
}

echo "=== ORIGINAL (spec'd) ==="
ORIG=$(build_and_check "$SRC" orig)
echo "original: $ORIG"
if [ "$ORIG" != "SUCCESS" ]; then
  echo "Original does not verify; kill testing meaningless."
  exit 1
fi

# Generate mutants
get-mutants --function ${FUNCTION} --file $SRC > $WORK/mutants.txt 2>/dev/null
# Split into individual diffs and apply each
python3 - "$SRC" "$WORK" <<'PY'
import sys,subprocess,re,os
src, work = sys.argv[1], sys.argv[2]
raw = open(os.path.join(work,"mutants.txt")).read()
# Each mutant is a unified diff block starting with '--- original'
blocks = re.split(r'(?=^--- original)', raw, flags=re.M)
blocks = [b for b in blocks if b.strip().startswith('--- original')]
orig_lines = open(src).read().split('\n')
killed=0; survived=0; errs=0
results=[]
for idx,b in enumerate(blocks):
    m = re.search(r'@@ -(\d+) \+(\d+) @@', b)
    if not m:
        continue
    ln = int(m.group(1))  # 1-based line in current file
    # find the '-' and '+' content lines (after the @@ line)
    lines = b.split('\n')
    minus=None; plus=None
    for L in lines:
        if L.startswith('-') and not L.startswith('--- '):
            minus = L[1:]
        elif L.startswith('+') and not L.startswith('+++ '):
            plus = L[1:]
    if minus is None or plus is None:
        continue
    new = list(orig_lines)
    # line ln is 1-based; verify it matches minus
    if new[ln-1] != minus:
        # try to locate
        results.append((idx,ln,'NOMATCH',minus.strip(),new[ln-1].strip()))
        continue
    new[ln-1] = plus
    mf = os.path.join(work, f"mut_{idx}.c")
    open(mf,'w').write('\n'.join(new))
    # build and check via the same bash function logic inline
    def run(cmd):
        return subprocess.run(cmd, shell=True, capture_output=True, text=True)
    F="OptimizeHuffmanForRle"; STUB="stubs/cprover_alloc.c"
    tag=f"mut_{idx}"
    r=run(f"goto-cc -o {work}/{tag}.goto {mf} {STUB} -I Syzygy_Zopfli/c_code --function {F}")
    if r.returncode!=0:
        results.append((idx,ln,'GOTOCC_ERR',plus.strip(),'')); errs+=1; continue
    run(f"goto-instrument --partial-loops --unwind 5 {work}/{tag}.goto {work}/{tag}.goto")
    run(f"goto-instrument --replace-call-with-contract AbsDiff --enforce-contract {F} {work}/{tag}.goto {work}/{tag}-c.goto")
    r=run(f"cbmc {work}/{tag}-c.goto --function {F} --depth 200")
    o=r.stdout+r.stderr
    if "VERIFICATION SUCCESSFUL" in o:
        results.append((idx,ln,'SURVIVED',plus.strip(),'')); survived+=1
    elif "VERIFICATION FAILED" in o:
        results.append((idx,ln,'KILLED',plus.strip(),'')); killed+=1
    else:
        results.append((idx,ln,'OTHER',plus.strip(),'')); errs+=1
    os.remove(mf)
for idx,ln,st,mut,extra in results:
    print(f"[{st}] line {ln}: {mut}  {extra}")
total=killed+survived
print(f"\nKILLED={killed} SURVIVED={survived} ERR={errs} TOTAL_MUT={len(blocks)} (scored={total})")
PY
