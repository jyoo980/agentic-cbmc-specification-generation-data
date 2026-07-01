#!/bin/sh
# Mutation testing harness for moveUp in 2048.c.
# Mirrors the real verification pipeline: replace slideArray with its contract,
# enforce moveUp's contract, run CBMC. Reports KILL/SURVIVE per mutant.
set -e
FUNCTION=moveUp
FILE=2048.c
WORK=$(mktemp -d)

mutate() {
  ln="$1"; new="$2"
  awk -v L="$ln" -v R="$new" 'NR==L{print R; next} {print}' "$FILE" > "$WORK/m.c"
  rm -f "$WORK/m.goto" "$WORK/check.goto"
  if ! goto-cc -o "$WORK/m.goto" "$WORK/m.c" cbmc_stubs.c --function $FUNCTION >/dev/null 2>&1; then
    echo "KILLED (compile error)"; return
  fi
  goto-instrument --partial-loops --unwind 5 "$WORK/m.goto" "$WORK/m.goto" >/dev/null 2>&1
  if ! goto-instrument --replace-call-with-contract slideArray --enforce-contract $FUNCTION "$WORK/m.goto" "$WORK/check.goto" >/dev/null 2>&1; then
    echo "KILLED (instrument error)"; return
  fi
  if cbmc "$WORK/check.goto" --function $FUNCTION --depth 200 2>&1 | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "SURVIVED"
  else
    echo "KILLED"
  fi
}

first_after() { awk -F: -v M="$2" '$1>M{print $1; exit}'; }
SIG=$(grep -n "^bool moveUp" "$FILE" | head -1 | cut -d: -f1)
L_FOR=$(grep -n "for (x = 0; x < SIZE; x++)" "$FILE" | first_after _ "$SIG")
echo "line: for=$L_FOR"

printf "x<=SIZE      : "; mutate "$L_FOR" "	for (x = 0; x <= SIZE; x++)"
printf "x>SIZE       : "; mutate "$L_FOR" "	for (x = 0; x > SIZE; x++)"
printf "x>=SIZE      : "; mutate "$L_FOR" "	for (x = 0; x >= SIZE; x++)"
printf "x==SIZE      : "; mutate "$L_FOR" "	for (x = 0; x == SIZE; x++)"
printf "x!=SIZE(eqv) : "; mutate "$L_FOR" "	for (x = 0; x != SIZE; x++)"

rm -rf "$WORK"
