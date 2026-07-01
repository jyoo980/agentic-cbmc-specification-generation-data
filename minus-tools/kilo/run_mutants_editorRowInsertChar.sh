#!/bin/bash
# Parallel mutation kill-score harness for editorRowInsertChar.
# Mirrors the CANONICAL run_cbmc pipeline EXACTLY:
#   goto-cc -> goto-instrument --partial-loops --unwind 5
#          -> goto-instrument --replace-call-with-contract editorUpdateRow
#                             --enforce-contract editorRowInsertChar
#          -> cbmc --depth <DEPTH>
# A "kill" means the verification of the spec FAILS on that mutant.
# Usage: ./run_mutants_editorRowInsertChar.sh [depth]
DEPTH=${1:-200}
FUNCTION=editorRowInsertChar
CALLEE=editorUpdateRow
SRC=/app/kilo/kilo.c
ROOT=$(mktemp -d)

# Line numbers resolved dynamically so the harness survives spec edits above the body.
GUARD_LINE=$(grep -n 'if (at > row->size) {' "$SRC" | cut -d: -f1)
PAD_LINE=$(grep -n 'int padlen = at-row->size;' "$SRC" | cut -d: -f1)
R1_LINE=$(grep -n 'row->chars = realloc(row->chars,row->size+padlen+2);' "$SRC" | cut -d: -f1)
MS_LINE=$(grep -n "memset(row->chars+row->size,' ',padlen);" "$SRC" | cut -d: -f1)
NUL_LINE=$(grep -n 'padlen+1\] = ' "$SRC" | cut -d: -f1)
SZ_LINE=$(grep -n 'row->size += padlen+1;' "$SRC" | cut -d: -f1)
R2_LINE=$(grep -n 'row->chars = realloc(row->chars,row->size+2);' "$SRC" | cut -d: -f1)
MM_LINE=$(grep -n 'memmove(row->chars+at+1,row->chars+at,row->size-at+1);' "$SRC" | cut -d: -f1)

# Each entry: "id|lineno|sed-substitution".  Exactly the 18 mutants emitted by
# `get-mutants --function editorRowInsertChar`.
declare -a MUTANTS=(
  # guard: if (at > row->size)
  "01_guard_lt|${GUARD_LINE}|s@at > row->size@at < row->size@"
  "02_guard_le|${GUARD_LINE}|s@at > row->size@at <= row->size@"
  "03_guard_ge|${GUARD_LINE}|s@at > row->size@at >= row->size@"
  "04_guard_eq|${GUARD_LINE}|s@at > row->size@at == row->size@"
  "05_guard_ne|${GUARD_LINE}|s@at > row->size@at != row->size@"
  # padding branch
  "06_pad_plus|${PAD_LINE}|s@at-row->size@at+row->size@"
  "07_r1_minus2|${R1_LINE}|s@row->size+padlen+2@row->size+padlen-2@"
  "08_r1_minuspad|${R1_LINE}|s@row->size+padlen+2@row->size-padlen+2@"
  "09_ms_minus|${MS_LINE}|s@row->chars+row->size@row->chars-row->size@"
  "10_nul_minus1|${NUL_LINE}|s@row->size+padlen+1@row->size+padlen-1@"
  "11_nul_minuspad|${NUL_LINE}|s@row->size+padlen+1@row->size-padlen+1@"
  "12_sz_minus1|${SZ_LINE}|s@padlen+1@padlen-1@"
  # else branch realloc
  "13_r2_minus2|${R2_LINE}|s@row->size+2@row->size-2@"
  # else branch memmove(row->chars+at+1, row->chars+at, row->size-at+1)
  "14_mm_n_minus1|${MM_LINE}|s@row->size-at+1)@row->size-at-1)@"
  "15_mm_n_plus|${MM_LINE}|s@row->size-at+1)@row->size+at+1)@"
  "16_mm_src_minus|${MM_LINE}|s@,row->chars+at,@,row->chars-at,@"
  "17_mm_dest_atm1|${MM_LINE}|s@row->chars+at+1,@row->chars+at-1,@"
  "18_mm_dest_minus|${MM_LINE}|s@row->chars+at+1,@row->chars-at+1,@"
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
  goto-instrument --replace-call-with-contract ${CALLEE} \
    --enforce-contract ${FUNCTION} m.goto c.goto >> log 2>&1
  cbmc c.goto --function ${FUNCTION} --depth ${DEPTH} > cbmc.out 2>&1
  if grep -q "VERIFICATION SUCCESSFUL" cbmc.out; then
    echo "$id: SURVIVED"
  elif grep -q "VERIFICATION FAILED" cbmc.out; then
    echo "$id: KILLED"
  else
    echo "$id: ERROR ($(tail -1 cbmc.out))"
  fi
}

echo "ROOT=$ROOT  DEPTH=$DEPTH"
for m in "${MUTANTS[@]}"; do
  IFS='|' read -r id line sub <<< "$m"
  run_one "$id" "$line" "$sub" >> "$ROOT/results.txt" 2>&1 &
done
wait
echo "=== RESULTS ==="
sort "$ROOT/results.txt"
killed=$(grep -c KILLED "$ROOT/results.txt")
surv=$(grep -c SURVIVED "$ROOT/results.txt")
echo "=== KILLED $killed / $((killed+surv)) (survived $surv) ==="
