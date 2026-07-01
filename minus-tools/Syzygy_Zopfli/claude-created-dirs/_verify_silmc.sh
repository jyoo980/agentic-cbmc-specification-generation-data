#!/bin/bash
# Replicates the avocado harness "missing-body retry" path (prevent_macro_expansion=True):
# goto-cc -D__NO_CTYPE ; goto-instrument --add-library (declares malloc, needed by is_fresh);
# unwind; replace-call-with-contract + enforce-contract; cbmc --depth 200.
set -u
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=StoreInLongestMatchCache
WORK=/app/_verify_silmc
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp ${1:-$SRC} mutated.c
goto-cc -D__NO_CTYPE -o m.goto mutated.c -I /app/Syzygy_Zopfli/c_code --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --add-library m.goto m.goto 2>/tmp/lib.err || { echo LIBFAIL; cat /tmp/lib.err; exit 1; }
goto-instrument --partial-loops --unwind 5 m.goto m.goto 2>/tmp/u.err || { echo UNWFAIL; cat /tmp/u.err; exit 1; }
goto-instrument --replace-call-with-contract ZopfliSublenToCache --enforce-contract $FUNCTION m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc mc.goto --function $FUNCTION --depth 200 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE"
