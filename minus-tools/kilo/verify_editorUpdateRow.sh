#!/bin/bash
# Verify editorUpdateRow with CBMC.
# Usage: ./verify_editorUpdateRow.sh [unwind]
set -e
UNWIND=${1:-10}
FUNCTION=editorUpdateRow
FILE=kilo.c
rm -f ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
goto-cc -o ${FUNCTION}.goto "$FILE" /app/stubs/ctype.c /app/stubs/string.c --function ${FUNCTION}
# editorUpdateSyntax is recursive (self-call on the E.syntax!=NULL path), which
# crashes the contract inliner.  It runs only after all of editorUpdateRow's
# render/alloc logic (where every mutant lives) has finished, so its body is
# irrelevant to this proof: drop kilo's body and link a no-op stub so the call
# returns normally (and the recursion disappears).
goto-instrument --remove-function-body editorUpdateSyntax ${FUNCTION}.goto ${FUNCTION}.goto
goto-cc -o stub_eus.goto stub_editorUpdateSyntax.c
# Model exit() as an assertion failure (instead of CBMC's default path-pruning
# no-return) so mutants that fire the "line too long" overflow guard spuriously
# are caught: under this contract (row->size == 1) exit is provably unreachable.
goto-cc -o stub_exit.goto stub_exit.c
goto-cc -o ${FUNCTION}.goto ${FUNCTION}.goto stub_eus.goto stub_exit.goto --function ${FUNCTION}
goto-instrument --unwind ${UNWIND} --unwinding-assertions ${FUNCTION}.goto ${FUNCTION}.goto
goto-instrument --nondet-static \
  --enforce-contract ${FUNCTION} ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
cbmc checking-${FUNCTION}-contracts.goto --function ${FUNCTION} \
  --no-malloc-may-fail --bounds-check --pointer-check --pointer-primitive-check \
  --pointer-overflow-check --conversion-check --signed-overflow-check --unsigned-overflow-check
