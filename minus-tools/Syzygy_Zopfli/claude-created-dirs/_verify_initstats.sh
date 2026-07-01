#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=InitStats
WORK=/app/_verify_initstats
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp $SRC mutated.c
goto-cc -o m.goto mutated.c -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --enforce-contract $FUNCTION m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc mc.goto --function $FUNCTION --depth 200 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|error|conversion|invariant"
