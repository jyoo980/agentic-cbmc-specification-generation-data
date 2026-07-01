#!/bin/bash
# Parallel mutation kill-score harness for editorDelChar.
# Mirrors the CANONICAL run_cbmc pipeline EXACTLY (see verify_editorDelChar.sh):
#   goto-cc -> goto-instrument --partial-loops --unwind 5
#          -> goto-instrument --replace-call-with-contract {the 4 contracted callees}
#                             --enforce-contract editorDelChar
#          -> cbmc --depth 200
# so a "kill" means the canonical verification of the spec fails on that mutant.
# Each mutant runs in its own temp dir so the runs proceed concurrently.
# Usage: ./run_mutants_editorDelChar.sh
FUNCTION=editorDelChar
SRC=/app/kilo/kilo.c
ROOT=$(mktemp -d)

# Resolve body line numbers dynamically so the harness survives edits to the spec.
# Scope every lookup to editorDelChar's body, since several of these statements
# also appear verbatim in editorInsertChar/editorInsertNewline.
DEF=$(grep -n 'void editorDelChar(void)' "$SRC" | cut -d: -f1)
END=$(awk -v s="$DEF" 'NR>s && /^int editorOpen\(char/ {print NR; exit}' "$SRC")
# grep within [DEF,END); returns the FIRST matching absolute line number.
gl() { awk -v a="$DEF" -v b="$END" 'NR>=a && NR<b' "$SRC" | grep -n -- "$1" | head -1 | cut -d: -f1 | awk -v a="$DEF" '{print $1 + a - 1}'; }
L_FILEROW=$(gl 'int filerow = E.rowoff+E.cy;')
L_FILECOL=$(gl 'int filecol = E.coloff+E.cx;')
L_ROW=$(gl 'erow \*row = (filerow >= E.numrows)')
L_GUARD=$(gl 'if (!row || (filecol == 0 && filerow == 0)) return;')
L_IFCOL=$(gl 'if (filecol == 0) {')
L_PREVSZ=$(gl 'filecol = E.row\[filerow-1\].size;')
L_APPEND=$(gl 'editorRowAppendString(&E.row\[filerow-1\],row->chars,row->size);')
L_CY0=$(gl 'if (E.cy == 0)')
L_CXSC=$(gl 'if (E.cx >= E.screencols) {')
L_SHIFT=$(gl 'int shift = (E.screencols-E.cx)+1;')
L_RDC=$(gl 'editorRowDelChar(row,filecol-1);')
L_CX0=$(gl 'if (E.cx == 0 && E.coloff)')

# Each entry: "id|lineno|sed-substitution".  The 25 mutants are exactly those
# emitted by `get-mutants --function editorDelChar`.
declare -a MUTANTS=(
  "01_filerow_sub|${L_FILEROW}|s@E.rowoff+E.cy@E.rowoff-E.cy@"
  "02_filecol_sub|${L_FILECOL}|s@E.coloff+E.cx@E.coloff-E.cx@"
  "03_row_lt|${L_ROW}|s@filerow >= E.numrows@filerow < E.numrows@"
  "04_row_le|${L_ROW}|s@filerow >= E.numrows@filerow <= E.numrows@"
  "05_row_gt|${L_ROW}|s@filerow >= E.numrows@filerow > E.numrows@"
  "06_row_eq|${L_ROW}|s@filerow >= E.numrows@filerow == E.numrows@"
  "07_row_ne|${L_ROW}|s@filerow >= E.numrows@filerow != E.numrows@"
  "08_guard_frne|${L_GUARD}|s@filecol == 0 && filerow == 0@filecol == 0 \&\& filerow != 0@"
  "09_guard_fcne|${L_GUARD}|s@filecol == 0 && filerow == 0@filecol != 0 \&\& filerow == 0@"
  "10_guard_and|${L_GUARD}|s@!row || (filecol@!row \&\& (filecol@"
  "11_guard_or2|${L_GUARD}|s@filecol == 0 && filerow == 0@filecol == 0 || filerow == 0@"
  "12_ifcol_ne|${L_IFCOL}|s@if (filecol == 0)@if (filecol != 0)@"
  "13_prevsz_plus|${L_PREVSZ}|s@E.row\[filerow-1\]@E.row[filerow+1]@"
  "14_append_plus|${L_APPEND}|s@&E.row\[filerow-1\]@\&E.row[filerow+1]@"
  "15_cy_ne|${L_CY0}|s@if (E.cy == 0)@if (E.cy != 0)@"
  "16_cxsc_lt|${L_CXSC}|s@E.cx >= E.screencols@E.cx < E.screencols@"
  "17_cxsc_le|${L_CXSC}|s@E.cx >= E.screencols@E.cx <= E.screencols@"
  "18_cxsc_gt|${L_CXSC}|s@E.cx >= E.screencols@E.cx > E.screencols@"
  "19_cxsc_eq|${L_CXSC}|s@E.cx >= E.screencols@E.cx == E.screencols@"
  "20_cxsc_ne|${L_CXSC}|s@E.cx >= E.screencols@E.cx != E.screencols@"
  "21_shift_minus|${L_SHIFT}|s@(E.screencols-E.cx)+1@(E.screencols-E.cx)-1@"
  "22_shift_plus|${L_SHIFT}|s@(E.screencols-E.cx)+1@(E.screencols+E.cx)+1@"
  "23_rdc_plus|${L_RDC}|s@editorRowDelChar(row,filecol-1)@editorRowDelChar(row,filecol+1)@"
  "24_cx0_ne|${L_CX0}|s@if (E.cx == 0 && E.coloff)@if (E.cx != 0 \&\& E.coloff)@"
  "25_cx0_or|${L_CX0}|s@if (E.cx == 0 && E.coloff)@if (E.cx == 0 || E.coloff)@"
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
    --replace-call-with-contract editorRowAppendString \
    --replace-call-with-contract editorDelRow \
    --replace-call-with-contract editorRowDelChar \
    --replace-call-with-contract editorUpdateRow \
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
echo "=== KILLED $killed / 25 (survived $surv) ==="
