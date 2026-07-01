#!/bin/bash
# Verify editorInsertNewline with the EXACT canonical run_cbmc pipeline (see
# /app/tools/run_cbmc.py):
#   goto-cc -> goto-instrument --partial-loops --unwind 5
#          -> goto-instrument --replace-call-with-contract {editorInsertRow,editorUpdateRow}
#                             --enforce-contract editorInsertNewline
#          -> cbmc --depth 200
# Usage: ./verify_editorInsertNewline.sh [depth]
set -u
DEPTH=${1:-200}
FUNCTION=editorInsertNewline
FILE=kilo.c
W=$(mktemp -d)
cp "$FILE" "$W/kilo.c"
cd "$W" || exit 99
goto-cc -o m.goto kilo.c --function ${FUNCTION} > log 2>&1 || { echo GOTOCC-FAIL; cat log; exit 1; }
goto-instrument --partial-loops --unwind 5 m.goto m.goto >> log 2>&1
goto-instrument --replace-call-with-contract editorInsertRow \
  --replace-call-with-contract editorUpdateRow \
  --enforce-contract ${FUNCTION} m.goto c.goto >> log 2>&1 || { echo INSTR-FAIL; cat log; exit 2; }
cbmc c.goto --function ${FUNCTION} --depth ${DEPTH} > cbmc.out 2>&1
echo "RC=$?  DEPTH=${DEPTH}"
grep -E "no body for|VERIFICATION|FAILURE|\*\* " cbmc.out | head -40
echo "WORKDIR=$W"
