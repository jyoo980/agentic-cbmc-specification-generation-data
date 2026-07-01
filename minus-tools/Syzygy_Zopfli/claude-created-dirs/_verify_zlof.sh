#!/bin/bash
# Usage: _verify_zlof.sh <src.c> <depth>
set -u
SRC=${1:-/app/Syzygy_Zopfli/c_code/zopfli.c}
DEPTH=${2:-200}
FUNCTION=ZopfliLZ77OptimalFixed
INC=/app/Syzygy_Zopfli/c_code
STUB=/app/stubs/cprover_alloc.c
WORK=/app/_zlof_work
rm -rf $WORK; mkdir -p $WORK; cd $WORK
goto-cc -o f.goto -I $INC "$SRC" $STUB --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --partial-loops --unwind 5 f.goto f.goto 2>/tmp/u.err || { echo UNWFAIL; cat /tmp/u.err; exit 1; }
goto-instrument --replace-call-with-contract LZ77OptimalRun \
                --replace-call-with-contract ZopfliAllocHash \
                --replace-call-with-contract ZopfliCleanHash \
                --enforce-contract $FUNCTION f.goto fc.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc fc.goto --function $FUNCTION --depth $DEPTH 2>&1 | grep -E "FAILURE|VERIFICATION|failed" | tail -8
