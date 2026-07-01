#!/bin/bash
# Replicate the canonical run_cbmc pipeline for editorFind.
# Usage: ./_canon_find.sh <SRC.c> [workdir]
set -u
SRC=${1:-/app/kilo/kilo.c}
W=${2:-$(mktemp -d)}
FN=editorFind
cd "$W" || exit 99
goto-cc -o $FN.goto "$SRC" --function $FN > gc.out 2>&1 || { echo "GOTO_CC_FAIL"; cat gc.out; exit 1; }
goto-instrument --partial-loops --unwind 5 $FN.goto $FN.goto > gi1.out 2>&1
goto-instrument --replace-call-with-contract editorReadKey --replace-call-with-contract editorRefreshScreen --replace-call-with-contract editorSetStatusMessage --enforce-contract $FN $FN.goto checking-$FN-contracts.goto > gi2.out 2>&1 || { echo "INSTR_FAIL"; cat gi2.out; exit 2; }
cbmc checking-$FN-contracts.goto --function $FN --depth 200 > cbmc.out 2>&1
RC=$?
echo "CBMC_RC=$RC"
grep -E "no body for|VERIFICATION|FAILURE|unwinding assertion|assertion" cbmc.out | head -60
echo "WORKDIR=$W"
