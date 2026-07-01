#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=ZopfliUpdateHash
WORK=/app/_verify_zuh
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o F.goto mutated.c -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --partial-loops --unwind 5 F.goto F.goto 2>/tmp/u.err || { echo UNWINDFAIL; cat /tmp/u.err; exit 1; }
goto-instrument --replace-call-with-contract UpdateHashValue --enforce-contract $FUNCTION F.goto checking.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc checking.goto --function $FUNCTION --depth 200 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE"
