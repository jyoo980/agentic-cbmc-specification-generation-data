#!/bin/bash
# Experimental: verify editorInsertRow letting CBMC do the unwinding (after
# slicing) instead of pre-unwinding the whole linked program with goto-instrument.
# Usage: ./verify_editorInsertRow2.sh [unwind]
set -e
UNWIND=${1:-5}
FUNCTION=editorInsertRow
FILE=kilo.c
rm -f ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
goto-cc -o ${FUNCTION}.goto "$FILE" --function ${FUNCTION}
goto-instrument --remove-function-body editorUpdateRow ${FUNCTION}.goto ${FUNCTION}.goto
goto-cc -o stub_editorUpdateRow.goto stub_editorUpdateRow.c
goto-cc -o realloc_model.goto realloc_model.c
goto-cc -o ${FUNCTION}.goto ${FUNCTION}.goto stub_editorUpdateRow.goto realloc_model.goto --function ${FUNCTION}
goto-instrument --nondet-static \
  --enforce-contract ${FUNCTION} ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
cbmc checking-${FUNCTION}-contracts.goto --function ${FUNCTION} \
  --unwind ${UNWIND} --unwinding-assertions --partial-loops \
  --drop-unused-functions \
  --no-malloc-may-fail --bounds-check --pointer-check --pointer-primitive-check \
  --pointer-overflow-check --conversion-check --signed-overflow-check --unsigned-overflow-check
