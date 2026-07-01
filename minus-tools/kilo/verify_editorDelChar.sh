#!/bin/bash
# Verify editorDelChar with CBMC, mirroring the CANONICAL run_cbmc pipeline:
#   goto-cc -> goto-instrument --partial-loops --unwind 5
#          -> goto-instrument --replace-call-with-contract <each contracted callee>
#                             --enforce-contract editorDelChar
#          -> cbmc --depth 200
# editorDelChar's contracted callees are editorRowAppendString, editorDelRow,
# editorRowDelChar and editorUpdateRow; the canonical pipeline replaces every
# contracted callee that is reachable, so we replace all four.
set -u
FUNCTION=editorDelChar
SRC=${1:-/app/kilo/kilo.c}
W=${2:-$(mktemp -d)}
cd "$W" || exit 99
cp "$SRC" kilo.c
goto-cc -o ${FUNCTION}.goto kilo.c --function ${FUNCTION} > gc.out 2>&1 || { echo "GOTOCC_FAIL"; cat gc.out; exit 1; }
goto-instrument --partial-loops --unwind 5 ${FUNCTION}.goto ${FUNCTION}.goto > gi1.out 2>&1
goto-instrument \
  --replace-call-with-contract editorRowAppendString \
  --replace-call-with-contract editorDelRow \
  --replace-call-with-contract editorRowDelChar \
  --replace-call-with-contract editorUpdateRow \
  --enforce-contract ${FUNCTION} ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto > gi2.out 2>&1 || { echo "INSTR_FAIL"; cat gi2.out; exit 2; }
cbmc checking-${FUNCTION}-contracts.goto --function ${FUNCTION} --depth 200 > cbmc.out 2>&1
RC=$?
echo "CBMC_RC=$RC"
grep -E "no body for|VERIFICATION|FAILURE|violated|failed" cbmc.out | head -40
echo "WORKDIR=$W"
