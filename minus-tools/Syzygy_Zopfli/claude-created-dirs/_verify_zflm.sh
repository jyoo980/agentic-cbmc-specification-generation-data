#!/bin/bash
# Verify (or kill-test) ZopfliFindLongestMatch, mirroring the avocado pipeline.
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=ZopfliFindLongestMatch
WORK=/app/_verify_zflm
DEPTH=${DEPTH:-200}
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -D__NO_CTYPE -o m.goto mutated.c -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --add-library m.goto m.goto 2>/tmp/lib.err || { echo LIBFAIL; cat /tmp/lib.err; exit 1; }
goto-instrument --partial-loops --unwind 5 m.goto m.goto 2>/dev/null
goto-instrument \
  --replace-call-with-contract TryGetFromLongestMatchCache \
  --replace-call-with-contract GetMatch \
  --replace-call-with-contract StoreInLongestMatchCache \
  --enforce-contract $FUNCTION m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc mc.goto --function $FUNCTION --depth $DEPTH 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE|line [0-9]+ .*FAILURE"
