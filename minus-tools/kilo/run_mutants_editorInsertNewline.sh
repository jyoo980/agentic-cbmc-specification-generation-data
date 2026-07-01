#!/bin/bash
# Parallel mutation kill-score harness for editorInsertNewline.
# Mirrors the CANONICAL run_cbmc pipeline EXACTLY (see verify_editorInsertNewline.sh
# and /app/tools/run_cbmc.py):
#   goto-cc -> goto-instrument --partial-loops --unwind 5
#          -> goto-instrument --replace-call-with-contract {editorInsertRow,editorUpdateRow}
#                             --enforce-contract editorInsertNewline
#          -> cbmc --depth 200
# so a "kill" means the canonical verification of the spec fails on that mutant.
# Each mutant runs in its own temp dir so the runs proceed concurrently.
# Usage: ./run_mutants_editorInsertNewline.sh
FUNCTION=editorInsertNewline
SRC=/app/kilo/kilo.c
ROOT=$(mktemp -d)

# Resolve body line numbers dynamically so the harness survives edits to the spec.
# Scope every lookup to editorInsertNewline's body (def .. next function), since
# several statements also appear verbatim in editorInsertChar/editorDelChar.
DEF=$(grep -n 'void editorInsertNewline(void)' "$SRC" | cut -d: -f1)
END=$(awk -v s="$DEF" 'NR>s && /^void editorDelChar\(void\)/ {print NR; exit}' "$SRC")
gl() { awk -v a="$DEF" -v b="$END" 'NR>=a && NR<b' "$SRC" | grep -n -- "$1" | head -1 | cut -d: -f1 | awk -v a="$DEF" '{print $1 + a - 1}'; }
L_FILEROW=$(gl 'int filerow = E.rowoff+E.cy;')
L_FILECOL=$(gl 'int filecol = E.coloff+E.cx;')
L_ROW=$(gl 'erow \*row = (filerow >= E.numrows)')
L_APPEND=$(gl 'if (filerow == E.numrows)')
L_CLAMP=$(gl 'if (filecol >= row->size)')
L_FCZERO=$(gl 'if (filecol == 0)')
L_SPLIT=$(gl 'editorInsertRow(filerow+1,row->chars+filecol,row->size-filecol);')
L_FIXCUR=$(gl 'if (E.cy == E.screenrows-1)')

# Each entry: "id|lineno|sed-substitution".  Exactly the mutants emitted by
# `get-mutants --function editorInsertNewline`.
declare -a MUTANTS=(
  "01_filerow_sub|${L_FILEROW}|s@E.rowoff+E.cy@E.rowoff-E.cy@"
  "02_filecol_sub|${L_FILECOL}|s@E.coloff+E.cx@E.coloff-E.cx@"
  "03_row_lt|${L_ROW}|s@filerow >= E.numrows@filerow < E.numrows@"
  "04_row_le|${L_ROW}|s@filerow >= E.numrows@filerow <= E.numrows@"
  "05_row_gt|${L_ROW}|s@filerow >= E.numrows@filerow > E.numrows@"
  "06_row_eq|${L_ROW}|s@filerow >= E.numrows@filerow == E.numrows@"
  "07_row_ne|${L_ROW}|s@filerow >= E.numrows@filerow != E.numrows@"
  "08_append_ne|${L_APPEND}|s@filerow == E.numrows@filerow != E.numrows@"
  "09_clamp_lt|${L_CLAMP}|s@filecol >= row->size@filecol < row->size@"
  "10_clamp_le|${L_CLAMP}|s@filecol >= row->size@filecol <= row->size@"
  "11_clamp_gt|${L_CLAMP}|s@filecol >= row->size@filecol > row->size@"
  "12_clamp_eq|${L_CLAMP}|s@filecol >= row->size@filecol == row->size@"
  "13_clamp_ne|${L_CLAMP}|s@filecol >= row->size@filecol != row->size@"
  "14_fczero_ne|${L_FCZERO}|s@filecol == 0@filecol != 0@"
  "15_split_sizeplus|${L_SPLIT}|s@row->size-filecol@row->size+filecol@"
  "16_split_charsminus|${L_SPLIT}|s@row->chars+filecol@row->chars-filecol@"
  "17_split_filerowminus|${L_SPLIT}|s@editorInsertRow(filerow+1,@editorInsertRow(filerow-1,@"
  "18_fixcur_ne|${L_FIXCUR}|s@E.cy == E.screenrows-1@E.cy != E.screenrows-1@"
  "19_fixcur_plus|${L_FIXCUR}|s@E.cy == E.screenrows-1@E.cy == E.screenrows+1@"
)

run_one() {
  local id="$1" line="$2" sub="$3"
  local d="$ROOT/$id"
  mkdir -p "$d"
  cp "$SRC" "$d/kilo.c"
  sed -i "${line}${sub}" "$d/kilo.c"
  if diff -q "$SRC" "$d/kilo.c" >/dev/null; then
    echo "$id: SED-NOOP (line $line)"; return
  fi
  cd "$d" || return
  if ! goto-cc -o m.goto kilo.c --function ${FUNCTION} > log 2>&1; then
    echo "$id: GOTOCC-FAIL (uncompilable mutant, excluded)"; return
  fi
  goto-instrument --partial-loops --unwind 5 m.goto m.goto >> log 2>&1
  goto-instrument \
    --replace-call-with-contract editorInsertRow \
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
echo "lines: filerow=$L_FILEROW filecol=$L_FILECOL row=$L_ROW append=$L_APPEND clamp=$L_CLAMP fczero=$L_FCZERO split=$L_SPLIT fixcur=$L_FIXCUR"
for m in "${MUTANTS[@]}"; do
  IFS='|' read -r id line sub <<< "$m"
  run_one "$id" "$line" "$sub" >> "$ROOT/results.txt" 2>&1 &
done
wait
echo "=== RESULTS ==="
sort "$ROOT/results.txt"
killed=$(grep -c KILLED "$ROOT/results.txt")
surv=$(grep -c SURVIVED "$ROOT/results.txt")
noop=$(grep -c SED-NOOP "$ROOT/results.txt")
echo "=== KILLED $killed / 19 (survived $surv, sed-noop $noop) ==="
