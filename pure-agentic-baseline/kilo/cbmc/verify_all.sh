#!/usr/bin/env bash
# Run CBMC contract verification for every function in kilo.c that carries a
# specification, using the appropriate enforcement flow for each:
#   - OLD   : goto-instrument --enforce-contract            (verify.sh)
#   - DFCC  : goto-instrument --dfcc --enforce-contract     (verify_dfcc.sh)
# DFCC is used for functions whose contracts contain a `frees` clause, which the
# older enforcement path does not support.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"

OLD=(is_separator editorSyntaxToColor editorFileWasModified
     editorRowHasOpenComment abAppend disableRawMode editorAtExit)
DFCC=(abFree editorFreeRow)

pass=0; fail=0
for fn in "${OLD[@]}"; do
    if "$HERE/verify.sh" "$fn" 30 >/dev/null 2>&1; then
        echo "PASS  (old ) $fn"; pass=$((pass+1))
    else
        echo "FAIL  (old ) $fn"; fail=$((fail+1))
    fi
done
for fn in "${DFCC[@]}"; do
    if "$HERE/verify_dfcc.sh" "$fn" 5 >/dev/null 2>&1; then
        echo "PASS  (dfcc) $fn"; pass=$((pass+1))
    else
        echo "FAIL  (dfcc) $fn"; fail=$((fail+1))
    fi
done
echo "-----------------------------"
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
