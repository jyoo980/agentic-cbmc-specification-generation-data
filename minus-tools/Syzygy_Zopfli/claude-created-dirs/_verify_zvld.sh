#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=ZopfliVerifyLenDist
HARNESS=/app/_zvld_harness.c
STUB=/app/stubs/cprover_alloc.c
WORK=/app/_verify_zvld
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o m.goto mutated.c $HARNESS $STUB -I /app/Syzygy_Zopfli/c_code --function __zvld_harness 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --dfcc __zvld_harness --enforce-contract $FUNCTION --apply-loop-contracts m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; tail -5 /tmp/inst.err; exit 1; }
cbmc mc.goto --function __zvld_harness --depth 200 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE"
