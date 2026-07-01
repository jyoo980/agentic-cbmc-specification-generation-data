#!/bin/bash
# Real-behaviour pipeline for editorProcessKeypress: identical to
# verify_editorProcessKeypress.sh EXCEPT editorMoveCursor is NOT replaced with
# its contract -- its real body is inlined.  This is sound (editorMoveCursor is
# correct code with a verified contract) and is the only way to make the paging
# block observable: editorMoveCursor's own is_fresh+range contract makes the
# paging path infeasible when *replaced* (the field range read through the fresh
# E.row pointer is not solver-controllable), so the postcondition is vacuous.
# Inlining the real body propagates the keypress effect on E.cy/E.rowoff exactly,
# which is what the functional ensures pin down.  A higher --depth than canonical
# 200 is required only because the inlined body lengthens the symbolic path; the
# spec itself hard-codes no depth.
#
# Usage: ./verify_editorProcessKeypress_inline.sh [file.c] [depth] [workdir]
set -e
FILE="${1:-/app/kilo/kilo.c}"
DEPTH="${2:-1200}"
WORK="${3:-/tmp/epki}"
mkdir -p "$WORK"
FN=editorProcessKeypress
G="$WORK/$FN.goto"
C="$WORK/checking-$FN-contracts.goto"
rm -f "$G" "$C"
goto-cc -D__NO_CTYPE -o "$G" "$FILE" /app/stubs/readkey.c --function $FN
goto-instrument --add-library "$G" "$G" 2>/dev/null
goto-instrument --partial-loops --unwind 5 "$G" "$G" 2>/dev/null
goto-instrument \
  --replace-call-with-contract editorDelChar \
  --replace-call-with-contract editorFind \
  --replace-call-with-contract editorInsertChar \
  --replace-call-with-contract editorInsertNewline \
  --replace-call-with-contract editorReadKey \
  --replace-call-with-contract editorSave \
  --enforce-contract $FN "$G" "$C" 2>/dev/null
cbmc "$C" --function $FN --depth "$DEPTH"
