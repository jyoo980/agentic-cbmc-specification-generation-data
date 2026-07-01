#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=ZopfliLZ77Greedy
HARNESS=/app/_greedy_harness.c
ALLOC=/app/stubs/cprover_alloc.c
WEAK=/app/stubs/greedy_callees.c
ZSLLD=/app/stubs/zslld_weak.c
WORK=/app/_verify_greedy
DEPTH=${DEPTH:-200}
# Callees replaced by weak (sound) stubs; GetLengthScore keeps its real contract.
STUBBED="ZopfliResetHash ZopfliWarmupHash ZopfliUpdateHash ZopfliFindLongestMatch ZopfliVerifyLenDist ZopfliStoreLitLenDist"
REPLACE="--replace-call-with-contract ZopfliResetHash --replace-call-with-contract ZopfliWarmupHash --replace-call-with-contract ZopfliUpdateHash --replace-call-with-contract ZopfliFindLongestMatch --replace-call-with-contract ZopfliVerifyLenDist --replace-call-with-contract ZopfliStoreLitLenDist --replace-call-with-contract GetLengthScore"

rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o base.goto mutated.c $ALLOC -I /app/Syzygy_Zopfli/c_code --function __greedy_harness $HARNESS 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
RMARGS=""
for f in $STUBBED; do RMARGS="$RMARGS --remove-function-body $f"; done
goto-instrument $RMARGS base.goto base.goto 2>/tmp/rm.err || { echo RMFAIL; tail -5 /tmp/rm.err; exit 1; }
goto-cc -o weak.goto $WEAK $ZSLLD -I /app/Syzygy_Zopfli/c_code 2>/tmp/wk.err || { echo WKFAIL; cat /tmp/wk.err; exit 1; }
goto-cc -o m.goto weak.goto base.goto --function __greedy_harness 2>/tmp/lk.err || { echo LKFAIL; cat /tmp/lk.err; exit 1; }
goto-instrument --dfcc __greedy_harness --enforce-contract $FUNCTION $REPLACE --apply-loop-contracts m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; tail -30 /tmp/inst.err; exit 1; }
cbmc mc.goto --function __greedy_harness --depth $DEPTH 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE" | head -40
