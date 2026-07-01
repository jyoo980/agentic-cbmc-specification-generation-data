#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=ZopfliAppendLZ77Store
CALLEE=ZopfliStoreLitLenDist
HARNESS=/app/_zalz_harness.c
STUB=/app/stubs/cprover_alloc.c
WEAK=/app/stubs/zslld_weak.c
WORK=/app/_verify_zalz
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
# Compile caller TU with the real callee body stripped, link a weak (sound) model.
goto-cc -o base.goto mutated.c $STUB -I /app/Syzygy_Zopfli/c_code --function __zalz_harness $HARNESS 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --remove-function-body $CALLEE base.goto base.goto 2>/tmp/rm.err || { echo RMFAIL; tail -5 /tmp/rm.err; exit 1; }
goto-cc -o weak.goto $WEAK -I /app/Syzygy_Zopfli/c_code 2>/tmp/wk.err || { echo WKFAIL; cat /tmp/wk.err; exit 1; }
goto-cc -o m.goto weak.goto base.goto --function __zalz_harness 2>/tmp/lk.err || { echo LKFAIL; cat /tmp/lk.err; exit 1; }
goto-instrument --dfcc __zalz_harness --enforce-contract $FUNCTION --replace-call-with-contract $CALLEE --apply-loop-contracts m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; tail -30 /tmp/inst.err; exit 1; }
cbmc mc.goto --function __zalz_harness 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE" | head -40
