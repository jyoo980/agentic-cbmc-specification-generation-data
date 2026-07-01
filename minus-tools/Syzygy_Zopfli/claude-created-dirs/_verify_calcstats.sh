#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=CalculateStatistics
CALLEE=ZopfliCalculateEntropy
WEAK=/app/stubs/calcstats_weak.c
WORK=/app/_verify_calcstats
DEPTH=${DEPTH:-200}
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
# Compile caller TU with the real callee body stripped, link a weak (sound) model.
goto-cc -o base.goto mutated.c -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --remove-function-body $CALLEE base.goto base.goto 2>/tmp/rm.err || { echo RMFAIL; tail -5 /tmp/rm.err; exit 1; }
goto-cc -o weak.goto $WEAK -I /app/Syzygy_Zopfli/c_code 2>/tmp/wk.err || { echo WKFAIL; cat /tmp/wk.err; exit 1; }
goto-cc -o m.goto weak.goto base.goto --function $FUNCTION 2>/tmp/lk.err || { echo LKFAIL; cat /tmp/lk.err; exit 1; }
goto-instrument --enforce-contract $FUNCTION --replace-call-with-contract $CALLEE m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; tail -30 /tmp/inst.err; exit 1; }
cbmc mc.goto --function $FUNCTION --depth $DEPTH 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE" | head -40
