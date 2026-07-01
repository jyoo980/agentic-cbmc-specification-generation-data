#!/bin/bash
# Verify editorInsertRow with CBMC.
# Usage: ./verify_editorInsertRow.sh [unwind]
set -e
UNWIND=${1:-10}
FUNCTION=editorInsertRow
FILE=kilo.c
rm -f ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
# NOTE: do NOT link /app/stubs/string.c here -- its realloc model returns the
# object in place (no growth), which is correct for editorUpdateRow's hl trick
# but would break editorInsertRow's realloc(E.row, (numrows+1)*sizeof(erow)).
# We rely on CBMC's built-in realloc/malloc/memcpy/memmove, which grow/copy and
# bounds-check correctly.
goto-cc -o ${FUNCTION}.goto "$FILE" --function ${FUNCTION}
# Stub editorUpdateRow: remove kilo's (recursive-via-editorUpdateSyntax) body --
# whose hl-is-fresh contract is incompatible with editorInsertRow's hl=NULL -- and
# link a no-op that only dereferences its argument (to catch a corrupted pointer).
goto-instrument --remove-function-body editorUpdateRow ${FUNCTION}.goto ${FUNCTION}.goto
goto-cc -o stub_editorUpdateRow.goto stub_editorUpdateRow.c
# Faithful realloc model (see realloc_model.c): the default CBMC realloc havocs
# contents and leaves the grown array un-assignable in the caller's frame.
goto-cc -o realloc_model.goto realloc_model.c
goto-cc -o ${FUNCTION}.goto ${FUNCTION}.goto stub_editorUpdateRow.goto realloc_model.goto --function ${FUNCTION}
goto-instrument --unwind ${UNWIND} --unwinding-assertions ${FUNCTION}.goto ${FUNCTION}.goto
goto-instrument --nondet-static \
  --enforce-contract ${FUNCTION} ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
cbmc checking-${FUNCTION}-contracts.goto --function ${FUNCTION} \
  --no-malloc-may-fail --bounds-check --pointer-check --pointer-primitive-check \
  --pointer-overflow-check --conversion-check --signed-overflow-check --unsigned-overflow-check
