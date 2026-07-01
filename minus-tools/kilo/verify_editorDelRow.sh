#!/bin/bash
# Verify editorDelRow with CBMC.
# Usage: ./verify_editorDelRow.sh [unwind]
set -e
# The CBMC built-in memmove copies byte-by-byte in an internal loop, so --unwind
# must exceed the largest copy this harness performs: deleting row 0 of three
# shifts two erow structs = 2*sizeof(erow) = 96 bytes.  128 covers that with
# margin while the editor's own idx loop runs at most a couple of iterations.
UNWIND=${1:-128}
FUNCTION=editorDelRow
FILE=kilo.c
rm -f ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
# editorDelRow calls editorFreeRow (replaced by its contract) and memmove
# (CBMC built-in, which copies and bounds-checks correctly). No realloc here,
# so we do NOT link any string.c stub.
goto-cc -o ${FUNCTION}.goto "$FILE" --function ${FUNCTION}
goto-instrument --unwind ${UNWIND} --unwinding-assertions ${FUNCTION}.goto ${FUNCTION}.goto
# Inline editorFreeRow's body (three free() calls) rather than replacing it with
# its contract: the contract's `ensures __CPROVER_was_freed(...)` has no body
# under --replace-call-with-contract.  The precondition makes each row buffer a
# fresh, offset-zero heap object, so the inlined free()s are well-defined and
# editorDelRow's frees clause is checked against the real deallocations.
goto-instrument --nondet-static \
  --enforce-contract ${FUNCTION} ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
cbmc checking-${FUNCTION}-contracts.goto --function ${FUNCTION} \
  --no-malloc-may-fail --bounds-check --pointer-check --pointer-primitive-check \
  --pointer-overflow-check --conversion-check --signed-overflow-check --unsigned-overflow-check
