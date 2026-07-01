#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=FollowPath
WORK=/app/_verify_fp
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o m.goto mutated.c -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
# --add-library declares malloc etc., needed for __CPROVER_is_fresh in the
# requires clause to allocate (otherwise the requires are spuriously vacuous).
goto-instrument --add-library m.goto m.goto 2>/tmp/lib.err || { echo LIBFAIL; cat /tmp/lib.err; exit 1; }
# Both in-function loops are empty in the verified regime (instart==0 => warm-up
# loop empty, pathsize==0 => path loop empty).  A single SOUND unwind (no
# --partial-loops) strips them so the legacy enforce-contract assigns-checker
# sees a loop-free body; any mutant forcing >0 iterations trips the unwinding
# assertion or executes the body, rather than being silently skipped.
goto-instrument --unwind 1 --unwinding-assertions m.goto m.goto 2>/tmp/u.err || { echo UNWFAIL; cat /tmp/u.err; exit 1; }
goto-instrument --replace-call-with-contract ZopfliResetHash \
                --replace-call-with-contract ZopfliWarmupHash \
                --replace-call-with-contract ZopfliUpdateHash \
                --replace-call-with-contract ZopfliFindLongestMatch \
                --replace-call-with-contract ZopfliVerifyLenDist \
                --replace-call-with-contract ZopfliStoreLitLenDist \
                --enforce-contract $FUNCTION m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc mc.goto --function $FUNCTION --depth 4000 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE"
