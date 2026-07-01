#!/bin/bash
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=GetStatistics
WORK=/app/_verify_getstats
DEPTH=${DEPTH:-200}
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -o m.goto mutated.c -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --partial-loops --unwind 5 m.goto m.goto 2>/dev/null
goto-instrument --enforce-contract $FUNCTION \
  --replace-call-with-contract ZopfliGetLengthSymbol \
  --replace-call-with-contract ZopfliGetDistSymbol \
  --replace-call-with-contract CalculateStatistics \
  m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; tail -30 /tmp/inst.err; exit 1; }
cbmc mc.goto --function $FUNCTION --depth $DEPTH 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE" | head -40
