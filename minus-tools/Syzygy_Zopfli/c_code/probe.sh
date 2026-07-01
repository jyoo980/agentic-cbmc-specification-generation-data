#!/bin/sh
# Replace the first __CPROVER_ensures block (if-branch) with: (PROBE) ==> (1==0)
# If verification FAILS, the PROBE condition is reachable/satisfiable.
set -e
[ -f /tmp/z.orig ] || cp zopfli.c /tmp/z.orig
cp /tmp/z.orig zopfli.c
PROBE="$1"
PROBE="$PROBE" python3 - <<'PY'
import os
probe=os.environ["PROBE"]
f="zopfli.c"; L=open(f).read().split('\n')
assert L[374].strip().startswith("__CPROVER_ensures("), L[374]
L[374:383]=["__CPROVER_ensures( ("+probe+") ==> (1==0) )"]
open(f,'w').write('\n'.join(L))
PY
echo "PROBE: $PROBE"
./verify.sh BoundaryPMFinal 2>&1 | grep -iE "postcondition|failed \(|SUCCESSFUL|FAILED" | tail -4
cp /tmp/z.orig zopfli.c
