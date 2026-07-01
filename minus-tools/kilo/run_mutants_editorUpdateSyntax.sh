#!/bin/bash
# Mutation kill-score harness for editorUpdateSyntax.
# Parses the unified diffs from get-mutants, applies each mutant as a
# single-line replacement, runs the verification pipeline, and reports
# KILLED (verification fails) vs SURVIVED (verification succeeds).
# Usage: ./run_mutants_editorUpdateSyntax.sh [unwind] [depth]
UNWIND=${1:-6}
DEPTH=${2:-400}
FILE=kilo.c
FUNCTION=editorUpdateSyntax

get-mutants --function ${FUNCTION} --file ${FILE} > /tmp/muts_eus.txt 2>&1
cp "$FILE" "${FILE}.orig"

python3 - "$UNWIND" "$DEPTH" <<'PY'
import subprocess, sys, re
unwind, depth = sys.argv[1], sys.argv[2]
text = open('/tmp/muts_eus.txt').read()
orig_src = open('kilo.c.orig').read()
blocks = text.split('--- original')
mutants = []
for b in blocks:
    lines = b.splitlines()
    o = m = None
    seen_hunk = False
    for ln in lines:
        if ln.startswith('@@'):
            seen_hunk = True
            continue
        if not seen_hunk:
            continue
        if ln.startswith('-') and o is None:
            o = ln[1:]
        elif ln.startswith('+') and m is None:
            m = ln[1:]
    if o is not None and m is not None:
        mutants.append((o, m))

killed = survived = 0
surv_list = []
for idx,(o,m) in enumerate(mutants, 1):
    if o not in orig_src:
        print(f"MUTANT {idx}: ORIG-NOT-FOUND: {o!r}")
        continue
    if orig_src.count(o) != 1:
        print(f"MUTANT {idx}: ORIG-NOT-UNIQUE ({orig_src.count(o)}x): {o!r}")
        continue
    mutated = orig_src.replace(o, m, 1)
    open('kilo.c','w').write(mutated)
    r = subprocess.run(['./verify_editorUpdateSyntax.sh', unwind, depth],
                       capture_output=True, text=True)
    out = r.stdout + r.stderr
    if 'VERIFICATION SUCCESSFUL' in out:
        survived += 1
        surv_list.append((idx,o,m))
        print(f"MUTANT {idx}: SURVIVED  | {o.strip()}  ==>  {m.strip()}")
    else:
        killed += 1
        print(f"MUTANT {idx}: KILLED    | {o.strip()}  ==>  {m.strip()}")

open('kilo.c','w').write(orig_src)
print(f"\n=== KILLED {killed} / {killed+survived} ===")
print("\nSURVIVORS:")
for idx,o,m in surv_list:
    print(f"  [{idx}] {o.strip()}  ==>  {m.strip()}")
PY

cp "${FILE}.orig" "$FILE"
rm -f "${FILE}.orig"
