#!/bin/bash
# Parallel mutation kill-score harness for editorRowDelChar.
# Mirrors the CANONICAL run_cbmc pipeline EXACTLY (tools/run_cbmc.py):
#   goto-cc -> goto-instrument --partial-loops --unwind 5
#          -> goto-instrument --replace-call-with-contract editorUpdateRow
#                             --enforce-contract editorRowDelChar
#          -> cbmc --depth 200
# so a "kill" means the canonical verification of the spec fails on that mutant.
# Each mutant runs in its own temp dir so the runs proceed concurrently.
# Usage: ./run_mutants_editorRowDelChar.sh
FUNCTION=editorRowDelChar
CALLEE=editorUpdateRow
SRC=/app/kilo/kilo.c
ROOT=$(mktemp -d)

# Line numbers are resolved dynamically so the harness survives edits to the spec
# above the body.
GUARD_LINE=$(grep -n 'if (row->size <= at) return;' "$SRC" | cut -d: -f1)
MM_LINE=$(grep -n 'memmove(row->chars+at,row->chars+at+1,row->size-at);' "$SRC" | cut -d: -f1)

# Each entry: "id|lineno|sed-substitution".  The 9 mutants are exactly those
# emitted by `get-mutants --function editorRowDelChar`.
declare -a MUTANTS=(
  # guard: if (row->size <= at) return;
  "01_guard_lt|${GUARD_LINE}|s@row->size <= at@row->size < at@"
  "02_guard_gt|${GUARD_LINE}|s@row->size <= at@row->size > at@"
  "03_guard_ge|${GUARD_LINE}|s@row->size <= at@row->size >= at@"
  "04_guard_eq|${GUARD_LINE}|s@row->size <= at@row->size == at@"
  "05_guard_ne|${GUARD_LINE}|s@row->size <= at@row->size != at@"
  # memmove(row->chars+at, row->chars+at+1, row->size-at);
  "06_mm_size_plus|${MM_LINE}|s@,row->size-at)@,row->size+at)@"
  "07_mm_src_atm1|${MM_LINE}|s@,row->chars+at+1,@,row->chars+at-1,@"
  "08_mm_src_minus|${MM_LINE}|s@,row->chars+at+1,@,row->chars-at+1,@"
  "09_mm_dest_minus|${MM_LINE}|s@memmove(row->chars+at,@memmove(row->chars-at,@"
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
echo "=== KILLED $killed / $((killed+surv)) (survived $surv) ==="
