#!/bin/sh
# Mutation testing harness for findPairDown in 2048.c.
# For each official mutant (a line replacement), apply it, run CBMC enforcement,
# and report whether the spec KILLS it (verification fails) or it SURVIVES.
set -e
FUNCTION=findPairDown
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

# Find current line numbers of the mutated statements WITHIN findPairDown's body.
first_after() { awk -F: -v M="$2" '$1>M{print $1; exit}'; }
SIG=$(grep -n "^bool findPairDown" "$FILE" | head -1 | cut -d: -f1)
L_IF=$(grep -n "if (board\[x\]\[y\] == board\[x\]\[y + 1\])" "$FILE" | first_after _ "$SIG")
L_FY=$(grep -n "for (y = 0; y < SIZE - 1; y++)" "$FILE" | first_after _ "$SIG")
L_FX=$(grep -n "for (x = 0; x < SIZE; x++)" "$FILE" | first_after _ "$SIG")
echo "lines: if=$L_IF fy=$L_FY fx=$L_FX"

printf "bd!=        : "; mutate "$L_IF" "			if (board[x][y] != board[x][y + 1])"
printf "bd y-1      : "; mutate "$L_IF" "			if (board[x][y] == board[x][y - 1])"
printf "y<=SIZE-1   : "; mutate "$L_FY" "		for (y = 0; y <= SIZE - 1; y++)"
printf "y>SIZE-1    : "; mutate "$L_FY" "		for (y = 0; y > SIZE - 1; y++)"
printf "y>=SIZE-1   : "; mutate "$L_FY" "		for (y = 0; y >= SIZE - 1; y++)"
printf "y==SIZE-1   : "; mutate "$L_FY" "		for (y = 0; y == SIZE - 1; y++)"
printf "y!=SIZE-1   : "; mutate "$L_FY" "		for (y = 0; y != SIZE - 1; y++)"
printf "y<SIZE+1    : "; mutate "$L_FY" "		for (y = 0; y < SIZE + 1; y++)"
printf "x<=SIZE     : "; mutate "$L_FX" "	for (x = 0; x <= SIZE; x++)"
printf "x>SIZE      : "; mutate "$L_FX" "	for (x = 0; x > SIZE; x++)"
printf "x>=SIZE     : "; mutate "$L_FX" "	for (x = 0; x >= SIZE; x++)"
printf "x==SIZE     : "; mutate "$L_FX" "	for (x = 0; x == SIZE; x++)"
printf "x!=SIZE     : "; mutate "$L_FX" "	for (x = 0; x != SIZE; x++)"

rm -rf "$WORK"
