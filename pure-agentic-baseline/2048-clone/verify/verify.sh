#!/bin/bash
# Verify one function's CBMC contract via DFCC enforcement.
# Usage: verify.sh <function> <harness> <unwind> [extra cbmc flags...]
set -u
SRC=/app/2048-clone/2048.c
HARN=/app/2048-clone/verify/harness.c
STUBS=/app/2048-clone/verify/stubs.c
WORK=/app/2048-clone/verify
fn="$1"; harness="$2"; unwind="${3:-16}"; shift 3 || true
extra="$*"

goto-cc --function "$harness" -o "$WORK/all.goto" "$SRC" "$HARN" "$STUBS" 2>"$WORK/cc.err"
if [ $? -ne 0 ]; then echo "$fn: COMPILE FAILED"; cat "$WORK/cc.err"; exit 1; fi

# Replace findTarget/slideArray with contracts is avoided: we inline them.
goto-instrument --dfcc "$harness" --enforce-contract "$fn" \
    "$WORK/all.goto" "$WORK/mod.goto" 2>"$WORK/inst.err"
if [ $? -ne 0 ]; then echo "$fn: INSTRUMENT FAILED"; grep -iE "reason|error" "$WORK/inst.err" | head; exit 1; fi

cbmc --function "$harness" --unwind "$unwind" --unwinding-assertions $extra \
    "$WORK/mod.goto" >"$WORK/cbmc.out" 2>"$WORK/cbmc.err"
res=$(grep -E "VERIFICATION (SUCCESSFUL|FAILED)" "$WORK/cbmc.out" | tail -1)
echo "$fn: ${res:-NO RESULT}"
if echo "$res" | grep -q FAILED; then
  grep -E "FAILURE" "$WORK/cbmc.out" | head -8
fi
