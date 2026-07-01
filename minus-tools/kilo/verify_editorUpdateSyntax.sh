#!/bin/bash
# Verify editorUpdateSyntax with CBMC.
# Usage: ./verify_editorUpdateSyntax.sh [unwind] [depth]
set -e
UNWIND=${1:-6}
DEPTH=${2:-400}
FUNCTION=editorUpdateSyntax
FILE=kilo.c
rm -f ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
goto-cc -o ${FUNCTION}.goto "$FILE" /app/stubs/ctype.c /app/stubs/string.c --function ${FUNCTION}
goto-instrument --partial-loops --unwind ${UNWIND} ${FUNCTION}.goto ${FUNCTION}.goto
goto-instrument --nondet-static \
  --replace-call-with-contract is_separator \
  --replace-call-with-contract editorRowHasOpenComment \
  --replace-call-with-contract ${FUNCTION} \
  --enforce-contract ${FUNCTION} ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
cbmc checking-${FUNCTION}-contracts.goto --function ${FUNCTION} --depth ${DEPTH} \
  --no-malloc-may-fail --bounds-check --pointer-check --pointer-primitive-check \
  --pointer-overflow-check --conversion-check --signed-overflow-check --unsigned-overflow-check
