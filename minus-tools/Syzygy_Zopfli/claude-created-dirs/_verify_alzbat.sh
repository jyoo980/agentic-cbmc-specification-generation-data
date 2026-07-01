#!/bin/bash
# Verify AddLZ77BlockAutoType (empty-block regime).
# Usage: _verify_alzbat.sh <src.c> <depth>
set -u
SRC=${1:-/app/Syzygy_Zopfli/c_code/zopfli.c}
DEPTH=${2:-200}
FUNCTION=AddLZ77BlockAutoType
INC=/app/Syzygy_Zopfli/c_code
STUB=/app/stubs/cprover_alloc.c
WORK=/app/_alzbat_work
# Callees replaced by contract (AddBits & ZopfliInitLZ77Store left inlined).
CALLEES="ZopfliCalculateBlockSize ZopfliLZ77GetByteRange ZopfliInitBlockState ZopfliLZ77OptimalFixed ZopfliCleanBlockState AddLZ77Block ZopfliCleanLZ77Store"
rm -rf $WORK; mkdir -p $WORK; cd $WORK
cp "$SRC" mutated.c
goto-cc -o f.goto -I $INC mutated.c $STUB --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; exit 1; }
goto-instrument --partial-loops --unwind 5 f.goto f.goto 2>/tmp/u.err || { echo UNWFAIL; cat /tmp/u.err; exit 1; }
REPLACE=""
for c in $CALLEES; do REPLACE="$REPLACE --replace-call-with-contract $c"; done
goto-instrument $REPLACE --enforce-contract $FUNCTION f.goto fc.goto 2>/tmp/inst.err || { echo INSTFAIL; cat /tmp/inst.err; exit 1; }
cbmc fc.goto --function $FUNCTION --depth $DEPTH 2>&1 | grep -E "FAILURE|VERIFICATION|failed" | tail -12
