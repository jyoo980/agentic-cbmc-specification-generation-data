#!/bin/bash
# Measure the mutation kill score for editorSelectSyntaxHighlight's CBMC spec.
# Faithfully mirrors tools/run_cbmc.py: attempt 1 without --add-library; on a
# "no body for" warning (strlen/strstr have no body) retry with -D__NO_CTYPE +
# --add-library; the final attempt's verdict is authoritative. A mutant is KILLED
# iff the authoritative verdict is not VERIFICATION SUCCESSFUL.
# Usage: ./run_mutants_editorSelectSyntaxHighlight.sh [depth]
set -u
DEPTH=${1:-200}
F=editorSelectSyntaxHighlight
FILE=kilo.c
MDIR=_ess_mut
PYTHONPATH=/app /app/.venv/bin/python /app/eval/mutants/mutate_function.py \
    --function "$F" --file "$FILE" --out-dir "$MDIR" >/dev/null 2>&1

run_pipeline() { # $1 = src file ; echoes final verdict line
  local SRC=$1 W; W=$(mktemp -d)
  # attempt 1: no add-library
  ( cd "$W"
    goto-cc -o $F.goto "/app/kilo/$SRC" --function $F >gc1 2>&1 || { echo GOTO_CC_FAIL; exit; }
    goto-instrument --partial-loops --unwind 5 $F.goto $F.goto >gi1 2>&1
    goto-instrument --enforce-contract $F $F.goto chk.goto >gi2 2>&1 || { echo INSTR_FAIL; exit; }
    out=$(cbmc chk.goto --function $F --depth $DEPTH 2>&1)
    if echo "$out" | grep -q "VERIFICATION SUCCESSFUL"; then echo SUCCESS; exit; fi
    if echo "$out" | grep -q "no body for"; then
      # attempt 2: -D__NO_CTYPE + --add-library
      goto-cc -D__NO_CTYPE -o $F.goto "/app/kilo/$SRC" --function $F >gc2 2>&1 || { echo GOTO_CC_FAIL; exit; }
      goto-instrument --add-library $F.goto $F.goto >gl 2>&1
      goto-instrument --partial-loops --unwind 5 $F.goto $F.goto >gi3 2>&1
      goto-instrument --enforce-contract $F $F.goto chk.goto >gi4 2>&1 || { echo INSTR_FAIL; exit; }
      out=$(cbmc chk.goto --function $F --depth $DEPTH 2>&1)
      echo "$out" | grep -q "VERIFICATION SUCCESSFUL" && echo SUCCESS || echo FAIL
      exit
    fi
    echo FAIL )
  rm -rf "$W"
}

# Sanity: baseline must verify (SUCCESS).
base=$(run_pipeline "$FILE")
echo "BASELINE: $base"

killed=0; total=0
for m in "$MDIR"/*.c; do
  total=$((total+1))
  # copy mutant into kilo dir under a temp name so includes resolve
  cp "$m" "/app/kilo/_ess_cur.c"
  v=$(run_pipeline "_ess_cur.c")
  if [ "$v" = "SUCCESS" ]; then
    echo "SURVIVED  $(basename "$m")"
  else
    echo "KILLED($v) $(basename "$m")"
    killed=$((killed+1))
  fi
done
rm -f /app/kilo/_ess_cur.c
echo "=== KILLED $killed / $total (depth $DEPTH) ==="
