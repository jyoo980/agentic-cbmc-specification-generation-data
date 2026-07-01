#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=GetBestLengths
WORK=/app/_verify_gbl
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o m.goto mutated.c -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --add-library m.goto m.goto 2>/tmp/lib.err || { echo LIBFAIL; cat /tmp/lib.err; exit 1; }
goto-instrument --partial-loops --unwind 5 m.goto m.goto 2>/tmp/u.err || { echo UNWFAIL; cat /tmp/u.err; exit 1; }
goto-instrument --replace-call-with-contract GetCostModelMinCost \
                --replace-call-with-contract ZopfliResetHash \
                --replace-call-with-contract ZopfliWarmupHash \
                --replace-call-with-contract ZopfliUpdateHash \
                --replace-call-with-contract ZopfliFindLongestMatch \
                --replace-call-with-contract zopfli_min \
                --enforce-contract $FUNCTION m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc mc.goto --function $FUNCTION --depth 200 2>&1 | tail -20
