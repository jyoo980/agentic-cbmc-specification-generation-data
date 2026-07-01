#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=GetLengthScore
WORK=/app/_verify_gls
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o m.goto mutated.c -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --partial-loops --unwind 5 m.goto mu.goto 2>/tmp/unw.err || { echo UNWFAIL; cat /tmp/unw.err; exit 1; }
goto-instrument --enforce-contract $FUNCTION mu.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc mc.goto --function $FUNCTION --depth 200 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE"
