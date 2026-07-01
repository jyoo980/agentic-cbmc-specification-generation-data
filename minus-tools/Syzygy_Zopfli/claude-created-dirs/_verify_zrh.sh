#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=ZopfliResetHash
WORK=/app/_verify_zrh
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o m.goto mutated.c /app/_zrh_harness.c /app/stubs/cprover_alloc.c -I /app/Syzygy_Zopfli/c_code --function __zrh_harness 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --dfcc __zrh_harness --enforce-contract $FUNCTION --apply-loop-contracts m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc mc.goto --function __zrh_harness 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE"
