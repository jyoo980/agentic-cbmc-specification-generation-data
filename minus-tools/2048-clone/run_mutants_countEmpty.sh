#!/bin/sh
# Mutation testing harness for countEmpty in 2048.c.
# For each official mutant (a line replacement), apply it, run CBMC enforcement,
# and report whether the spec KILLS it (verification fails) or it SURVIVES.
set -e
FUNCTION=countEmpty
FILE=2048.c
WORK=$(mktemp -d)

mutate() {
  ln="$1"; new="$2"
  awk -v L="$ln" -v R="$new" 'NR==L{print R; next} {print}' "$FILE" > "$WORK/m.c"
  rm -f "$WORK/m.goto" "$WORK/check.goto"
  if ! goto-cc -o "$WORK/m.goto" "$WORK/m.c" cbmc_stubs.c --function $FUNCTION >/dev/null 2>&1; then
    echo "KILLED (compile error)"; return
  fi
  goto-instrument --unwind 5 --unwinding-assertions "$WORK/m.goto" "$WORK/m.goto" >/dev/null 2>&1
  if ! goto-instrument --enforce-contract $FUNCTION "$WORK/m.goto" "$WORK/check.goto" >/dev/null 2>&1; then
    echo "KILLED (instrument error)"; return
  fi
  if cbmc "$WORK/check.goto" --function $FUNCTION 2>&1 | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "SURVIVED"
  else
    echo "KILLED"
  fi
}

# Find current line numbers of the mutated statements WITHIN countEmpty's body
# (countEmpty's signature is at/after line 289).
first_after() { awk -F: -v M="$2" '$1>M{print $1; exit}'; }
L_EMPTY=$(grep -n "if (board\[x\]\[y\] == 0)" "$FILE" | first_after _ 289)
L_FY=$(grep -n "for (y = 0; y < SIZE; y++)" "$FILE" | first_after _ 289)
L_FX=$(grep -n "for (x = 0; x < SIZE; x++)" "$FILE" | first_after _ 289)
echo "lines: empty=$L_EMPTY fy=$L_FY fx=$L_FX"

printf "bd!=0   : "; mutate "$L_EMPTY" "			if (board[x][y] != 0)"
printf "y<=SIZE : "; mutate "$L_FY" "		for (y = 0; y <= SIZE; y++)"
printf "y>SIZE  : "; mutate "$L_FY" "		for (y = 0; y > SIZE; y++)"
printf "y>=SIZE : "; mutate "$L_FY" "		for (y = 0; y >= SIZE; y++)"
printf "y==SIZE : "; mutate "$L_FY" "		for (y = 0; y == SIZE; y++)"
printf "y!=SIZE : "; mutate "$L_FY" "		for (y = 0; y != SIZE; y++)"
printf "x<=SIZE : "; mutate "$L_FX" "	for (x = 0; x <= SIZE; x++)"
printf "x>SIZE  : "; mutate "$L_FX" "	for (x = 0; x > SIZE; x++)"
printf "x>=SIZE : "; mutate "$L_FX" "	for (x = 0; x >= SIZE; x++)"
printf "x==SIZE : "; mutate "$L_FX" "	for (x = 0; x == SIZE; x++)"
printf "x!=SIZE : "; mutate "$L_FX" "	for (x = 0; x != SIZE; x++)"

rm -rf "$WORK"
