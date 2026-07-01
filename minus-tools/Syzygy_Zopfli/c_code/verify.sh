#!/bin/sh
# Mirror the verify CBMC pipeline (unwind 5, depth 200, enforce-contract).
# Usage: ./verify.sh <FUNCTION> [<FILE>]
set -e
FUNCTION="$1"
FILE="${2:-zopfli.c}"
goto-cc -o "${FUNCTION}.goto" "${FILE}" --function "${FUNCTION}"
goto-instrument --partial-loops --unwind 5 "${FUNCTION}.goto" "${FUNCTION}.goto"
goto-instrument --enforce-contract "${FUNCTION}" "${FUNCTION}.goto" "checking-${FUNCTION}-contracts.goto"
cbmc "checking-${FUNCTION}-contracts.goto" --function "${FUNCTION}" --depth 200
