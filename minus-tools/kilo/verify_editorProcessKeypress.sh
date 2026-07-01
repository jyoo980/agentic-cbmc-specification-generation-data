#!/bin/bash
# Local CBMC pipeline for editorProcessKeypress.
#
# The CANONICAL avocado pipeline (tools/run_cbmc.py) is structurally blocked for
# this function: it replaces ALL in-file callees with their contracts, and
# replacing the *variadic* editorSetStatusMessage contract aborts goto-instrument
# 6.9.0 (invariant violation in std_expr.cpp `instantiate`, same crash as
# editorFind).  Additionally editorProcessKeypress never calls malloc, so
# goto-cc drops malloc's declaration and the is_fresh clauses in the other callee
# contracts (editorMoveCursor/editorInsertChar/...) cannot be instantiated
# ("function 'malloc' is not declared").
#
# This local script works around BOTH blockers while keeping everything else
# identical to canonical (unwind 5, --partial-loops, depth 200, readkey stub):
#   * `--add-library` re-declares malloc et al. so is_fresh instantiates,
#   * editorSetStatusMessage is NOT replaced -- its real body is inlined, which
#     is SOUND (it only havocs E.statusmsg/statusmsg_time via nondet vsnprintf).
# Every other callee is replaced with its contract exactly as canonical does.
#
# Usage: ./verify_editorProcessKeypress.sh [file.c] [workdir]
set -e
FILE="${1:-/app/kilo/kilo.c}"
WORK="${2:-/tmp/epk}"
mkdir -p "$WORK"
FN=editorProcessKeypress
G="$WORK/$FN.goto"
C="$WORK/checking-$FN-contracts.goto"
rm -f "$G" "$C"
goto-cc -D__NO_CTYPE -o "$G" "$FILE" /app/stubs/readkey.c --function $FN
goto-instrument --add-library "$G" "$G" 2>/dev/null
goto-instrument --partial-loops --unwind 5 "$G" "$G" 2>/dev/null
# Replace every in-file callee EXCEPT editorSetStatusMessage (variadic crash).
goto-instrument \
  --replace-call-with-contract editorDelChar \
  --replace-call-with-contract editorFind \
  --replace-call-with-contract editorInsertChar \
  --replace-call-with-contract editorInsertNewline \
  --replace-call-with-contract editorMoveCursor \
  --replace-call-with-contract editorReadKey \
  --replace-call-with-contract editorSave \
  --enforce-contract $FN "$G" "$C" 2>/dev/null
cbmc "$C" --function $FN --depth 200
