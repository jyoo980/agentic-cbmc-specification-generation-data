#!/bin/bash
# Probe: at what --depth does the false postcondition (E.dirty==999) get caught?
set -u
SRC=${1:-/tmp/vac.c}
FN=editorOpen
W=$(mktemp -d); cd "$W" || exit 99
goto-cc -o $FN.goto "$SRC" --function $FN >/dev/null 2>&1
goto-instrument --partial-loops --unwind 5 $FN.goto $FN.goto >/dev/null 2>&1
goto-instrument --replace-call-with-contract editorInsertRow --enforce-contract $FN $FN.goto checking.goto >/dev/null 2>&1
for d in 100 200 250 300 400 600 1000; do
  if cbmc checking.goto --function $FN --depth $d >/dev/null 2>&1; then
    echo "depth $d: VERIFIED (vacuous)"
  else
    echo "depth $d: FAILED (postcond reached)"
  fi
done
rm -rf "$W"
