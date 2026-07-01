#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=ZopfliAppendLZ77Store
CALLEE=ZopfliStoreLitLenDist
HARNESS=/app/_zalz_harness.c
STUB=/app/stubs/cprover_alloc.c
WORK=/app/_verify_zalz
DEPTH=${DEPTH:-200}
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o m.goto mutated.c $HARNESS $STUB -I /app/Syzygy_Zopfli/c_code --function __zalz_harness 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --dfcc __zalz_harness --enforce-contract $FUNCTION --replace-call-with-contract $CALLEE --apply-loop-contracts m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; tail -30 /tmp/inst.err; exit 1; }
cbmc mc.goto --function __zalz_harness  2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE|loop invariant|assertion" | head -40
