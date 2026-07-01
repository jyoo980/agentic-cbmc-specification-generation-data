#!/bin/bash
# Parallel mutation kill-score harness for editorDelRow.
# Mirrors verify_editorDelRow.sh EXACTLY (same unwind, same checks, same
# enforce-contract pipeline) so a "kill" means the canonical verification of the
# spec fails on that mutant.  Each mutant runs in its own temp dir so the 17
# runs proceed concurrently (the box has plenty of cores/RAM).
# Usage: ./run_mutants_editorDelRow.sh [unwind]
UNWIND=${1:-128}
FUNCTION=editorDelRow
SRC=/app/kilo/kilo.c
ROOT=$(mktemp -d)

# Each entry: "id|lineno|sed-substitution".  Substitutions are anchored to the
# exact original token so they apply to exactly one site on that line.
declare -a MUTANTS=(
  "01_guard_lt|867|s@at >= E\.numrows@at < E.numrows@"
  "02_guard_le|867|s@at >= E\.numrows@at <= E.numrows@"
  "03_guard_gt|867|s@at >= E\.numrows@at > E.numrows@"
  "04_guard_eq|867|s@at >= E\.numrows@at == E.numrows@"
  "05_guard_ne|867|s@at >= E\.numrows@at != E.numrows@"
  "06_row_minus|868|s@= E\.row+at;@= E.row-at;@"
  "07_mm_dest_minus|870|s@memmove(E\.row+at,@memmove(E.row-at,@"
  "08_mm_src_atm1|870|s@,E\.row+at+1,@,E.row+at-1,@"
  "09_mm_src_minus|870|s@,E\.row+at+1,@,E.row-at+1,@"
  "10_mm_size_atp1|870|s@(E\.numrows-at-1)@(E.numrows-at+1)@"
  "11_mm_size_plusat|870|s@(E\.numrows-at-1)@(E.numrows+at-1)@"
  "12_loop_le|871|s@j < E\.numrows-1@j <= E.numrows-1@"
  "13_loop_gt|871|s@j < E\.numrows-1@j > E.numrows-1@"
  "14_loop_ge|871|s@j < E\.numrows-1@j >= E.numrows-1@"
  "15_loop_eq|871|s@j < E\.numrows-1@j == E.numrows-1@"
  "16_loop_ne|871|s@j < E\.numrows-1@j != E.numrows-1@"
  "17_loop_plus1|871|s@j < E\.numrows-1@j < E.numrows+1@"
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
  goto-instrument --unwind ${UNWIND} --unwinding-assertions m.goto m.goto >> log 2>&1
  goto-instrument --nondet-static --enforce-contract ${FUNCTION} m.goto c.goto >> log 2>&1
  cbmc c.goto --function ${FUNCTION} \
    --no-malloc-may-fail --bounds-check --pointer-check --pointer-primitive-check \
    --pointer-overflow-check --conversion-check --signed-overflow-check \
    --unsigned-overflow-check > cbmc.out 2>&1
  if grep -q "VERIFICATION SUCCESSFUL" cbmc.out; then
    echo "$id: SURVIVED"
  elif grep -q "VERIFICATION FAILED" cbmc.out; then
    echo "$id: KILLED"
  else
    echo "$id: ERROR ($(tail -1 cbmc.out))"
  fi
}

echo "ROOT=$ROOT"
pids=()
for m in "${MUTANTS[@]}"; do
  IFS='|' read -r id line sub <<< "$m"
  run_one "$id" "$line" "$sub" >> "$ROOT/results.txt" 2>&1 &
  pids+=($!)
done
wait
echo "=== RESULTS ==="
sort "$ROOT/results.txt"
killed=$(grep -c KILLED "$ROOT/results.txt")
surv=$(grep -c SURVIVED "$ROOT/results.txt")
echo "=== KILLED $killed / $((killed+surv)) (survived $surv) ==="
