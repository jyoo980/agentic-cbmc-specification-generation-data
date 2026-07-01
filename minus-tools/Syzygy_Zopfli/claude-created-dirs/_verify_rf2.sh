#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=RandomizeFreqs
STUB=/app/stubs/cprover_alloc.c
WORK=/app/_verify_rf2
UNWIND=${UNWIND:-2}
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o m.goto mutated.c $STUB -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --add-library m.goto m.goto 2>/dev/null
goto-instrument --partial-loops --unwind $UNWIND m.goto m.goto 2>/dev/null
goto-instrument --enforce-contract $FUNCTION m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; tail -8 /tmp/inst.err; exit 1; }
cbmc mc.goto --function $FUNCTION --depth ${DEPTH:-200} 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE" | head
