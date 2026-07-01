#!/bin/bash
# Parallel mutation kill-score harness for editorInsertChar.
# Mirrors the CANONICAL run_cbmc pipeline EXACTLY (see verify_editorInsertChar.sh):
#   goto-cc -> goto-instrument --partial-loops --unwind 5
#          -> goto-instrument --replace-call-with-contract {editorInsertRow,editorRowInsertChar}
#                             --enforce-contract editorInsertChar
#          -> cbmc --depth 200
# so a "kill" means the canonical verification of the spec fails on that mutant.
# Each mutant runs in its own temp dir so the runs proceed concurrently.
# Usage: ./run_mutants_editorInsertChar.sh
FUNCTION=editorInsertChar
SRC=/app/kilo/kilo.c
ROOT=$(mktemp -d)

# Resolve body line numbers dynamically so the harness survives edits to the spec.
# Scope every lookup to editorInsertChar's body, since several of these statements
# also appear verbatim in editorDelChar/editorInsertNewline.
DEF=$(grep -n 'void editorInsertChar(int c)' "$SRC" | cut -d: -f1)
END=$(awk -v s="$DEF" 'NR>s && /^void editorInsertNewline\(void\)/ {print NR; exit}' "$SRC")
gl() { awk -v a="$DEF" -v b="$END" 'NR>=a && NR<b' "$SRC" | grep -n -- "$1" | head -1 | cut -d: -f1 | awk -v a="$DEF" '{print $1 + a - 1}'; }
L_FILEROW=$(gl 'int filerow = E.rowoff+E.cy;')
L_FILECOL=$(gl 'int filecol = E.coloff+E.cx;')
L_ROW=$(gl 'erow \*row = (filerow >= E.numrows)')
L_WHILE=$(gl 'while(E.numrows <= filerow)')
L_CXSC=$(gl 'if (E.cx == E.screencols-1)')

# Each entry: "id|lineno|sed-substitution".  The 14 mutants are exactly those
# emitted by `get-mutants --function editorInsertChar`.
declare -a MUTANTS=(
  "01_filerow_sub|${L_FILEROW}|s@E.rowoff+E.cy@E.rowoff-E.cy@"
  "02_filecol_sub|${L_FILECOL}|s@E.coloff+E.cx@E.coloff-E.cx@"
  "03_row_lt|${L_ROW}|s@filerow >= E.numrows@filerow < E.numrows@"
  "04_row_le|${L_ROW}|s@filerow >= E.numrows@filerow <= E.numrows@"
  "05_row_gt|${L_ROW}|s@filerow >= E.numrows@filerow > E.numrows@"
  "06_row_eq|${L_ROW}|s@filerow >= E.numrows@filerow == E.numrows@"
  "07_row_ne|${L_ROW}|s@filerow >= E.numrows@filerow != E.numrows@"
  "08_while_lt|${L_WHILE}|s@E.numrows <= filerow@E.numrows < filerow@"
  "09_while_gt|${L_WHILE}|s@E.numrows <= filerow@E.numrows > filerow@"
  "10_while_ge|${L_WHILE}|s@E.numrows <= filerow@E.numrows >= filerow@"
  "11_while_eq|${L_WHILE}|s@E.numrows <= filerow@E.numrows == filerow@"
  "12_while_ne|${L_WHILE}|s@E.numrows <= filerow@E.numrows != filerow@"
  "13_cxsc_ne|${L_CXSC}|s@E.cx == E.screencols-1@E.cx != E.screencols-1@"
  "14_cxsc_plus|${L_CXSC}|s@E.cx == E.screencols-1@E.cx == E.screencols+1@"
)

run_one() {
  local id="$1" line="$2" sub="$3"
  local d="$ROOT/$id"
  mkdir -p "$d"
  cp "$SRC" "$d/kilo.c"
  sed -i "${line}${sub}" "$d/kilo.c"
  if diff -q "$SRC" "$d/kilo.c" >/dev/null; then
    echo "$id: SED-NOOP"; return
  fi
  cd "$d" || return
  if ! goto-cc -o m.goto kilo.c --function ${FUNCTION} > log 2>&1; then
    echo "$id: GOTOCC-FAIL"; return
  fi
  goto-instrument --partial-loops --unwind 5 m.goto m.goto >> log 2>&1
  goto-instrument \
    --replace-call-with-contract editorInsertRow \
    --replace-call-with-contract editorRowInsertChar \
    --enforce-contract ${FUNCTION} m.goto c.goto >> log 2>&1
  cbmc c.goto --function ${FUNCTION} --depth 200 > cbmc.out 2>&1
  if grep -q "VERIFICATION SUCCESSFUL" cbmc.out; then
    echo "$id: SURVIVED"
  elif grep -q "VERIFICATION FAILED" cbmc.out; then
    echo "$id: KILLED"
  else
    echo "$id: ERROR ($(tail -1 cbmc.out))"
  fi
}

echo "ROOT=$ROOT"
for m in "${MUTANTS[@]}"; do
  IFS='|' read -r id line sub <<< "$m"
  run_one "$id" "$line" "$sub" >> "$ROOT/results.txt" 2>&1 &
done
wait
echo "=== RESULTS ==="
sort "$ROOT/results.txt"
killed=$(grep -c KILLED "$ROOT/results.txt")
surv=$(grep -c SURVIVED "$ROOT/results.txt")
echo "=== KILLED $killed / 14 (survived $surv) ==="
