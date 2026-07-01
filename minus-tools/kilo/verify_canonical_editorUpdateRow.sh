#!/bin/bash
# Replicate the *canonical* CBMC pipeline (tools/run_cbmc.py) for editorUpdateRow,
# so results match how the avocado harness scores the spec:
#   unwind = 5 (--partial-loops), depth = 200,
#   in-file callee editorUpdateSyntax discharged via --replace-call-with-contract,
#   stubs only from /app/stubs/*.c (none resolve for this function),
#   no --nondet-static, no explicit --*-check flags (CBMC defaults).
# Usage: ./verify_canonical_editorUpdateRow.sh <file.c> [workdir]
set -e
FILE="${1:-kilo.c}"
WORK="${2:-.}"
FUNCTION=editorUpdateRow
G="${WORK}/${FUNCTION}.goto"
C="${WORK}/checking-${FUNCTION}-contracts.goto"
rm -f "$G" "$C"
goto-cc -o "$G" "$FILE" --function ${FUNCTION}
goto-instrument --partial-loops --unwind 5 "$G" "$G"
goto-instrument --replace-call-with-contract editorUpdateSyntax \
  --enforce-contract ${FUNCTION} "$G" "$C"
cbmc "$C" --function ${FUNCTION} --depth 200
