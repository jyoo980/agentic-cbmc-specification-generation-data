#!/bin/bash
# Verify editorRowInsertChar with the EXACT canonical run_cbmc pipeline:
#   goto-cc -> goto-instrument --partial-loops --unwind 5
#          -> goto-instrument --replace-call-with-contract editorUpdateRow
#                             --enforce-contract editorRowInsertChar
#          -> cbmc --depth <DEPTH>
# Usage: ./verify_editorRowInsertChar.sh [depth]
set -u
DEPTH=${1:-200}
FUNCTION=editorRowInsertChar
CALLEE=editorUpdateRow
FILE=kilo.c
W=$(mktemp -d)
cp "$FILE" "$W/kilo.c"
cd "$W" || exit 99
goto-cc -o m.goto kilo.c --function ${FUNCTION} > log 2>&1 || { echo GOTOCC-FAIL; cat log; exit 1; }
goto-instrument --partial-loops --unwind 5 m.goto m.goto >> log 2>&1
goto-instrument --replace-call-with-contract ${CALLEE} \
  --enforce-contract ${FUNCTION} m.goto c.goto >> log 2>&1 || { echo INSTR-FAIL; cat log; exit 2; }
cbmc c.goto --function ${FUNCTION} --depth ${DEPTH} > cbmc.out 2>&1
echo "RC=$?  DEPTH=${DEPTH}"
grep -E "no body for|VERIFICATION|FAILURE|\*\* " cbmc.out | head -40
echo "WORKDIR=$W"
