#!/bin/sh
# Mirror the avocado run_cbmc pipeline for a (possibly self-recursive) function:
# callees' contracts are substituted via --replace-call-with-contract, including
# the function itself to break recursion (the recursion-inlining retry path).
# Usage: ./verify-rec.sh <FUNCTION> [<FILE>] [<CALLEE>...]
set -e
FUNCTION="$1"
FILE="${2:-zopfli.c}"
shift 2 || true
REPLACE=""
for c in "$@"; do
  REPLACE="$REPLACE --replace-call-with-contract $c"
done
goto-cc -o "${FUNCTION}.goto" "${FILE}" --function "${FUNCTION}"
goto-instrument --partial-loops --unwind 5 "${FUNCTION}.goto" "${FUNCTION}.goto"
goto-instrument $REPLACE --enforce-contract "${FUNCTION}" \
  "${FUNCTION}.goto" "checking-${FUNCTION}-contracts.goto"
cbmc "checking-${FUNCTION}-contracts.goto" --function "${FUNCTION}" --depth 200
