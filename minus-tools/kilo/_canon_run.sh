#!/bin/bash
# Replicate the EXACT canonical run_cbmc pipeline for editorOpen.
# Usage: ./_canon_run.sh <SRC.c> [workdir]
set -u
SRC=${1:-/app/kilo/kilo.c}
W=${2:-$(mktemp -d)}
FN=editorOpen
cd "$W" || exit 99
# Attempt 1: no macro prevention, no stubs.
goto-cc -o $FN.goto "$SRC" --function $FN > gc.out 2>&1 || { echo "GOTO_CC_FAIL"; cat gc.out; exit 1; }
goto-instrument --partial-loops --unwind 5 $FN.goto $FN.goto > gi1.out 2>&1
goto-instrument --replace-call-with-contract editorInsertRow --enforce-contract $FN $FN.goto checking-$FN-contracts.goto > gi2.out 2>&1 || { echo "INSTR_FAIL"; cat gi2.out; exit 2; }
cbmc checking-$FN-contracts.goto --function $FN --depth 200 > cbmc.out 2>&1
RC=$?
echo "CBMC_RC=$RC"
grep -E "no body for|VERIFICATION|FAILURE" cbmc.out | head -40
echo "WORKDIR=$W"
