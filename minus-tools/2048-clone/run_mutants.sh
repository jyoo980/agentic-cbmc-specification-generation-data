#!/bin/sh
# Mutation testing harness for addRandom in 2048.c.
# For each mutant (a line replacement), apply it, run CBMC enforcement, and
# report whether the spec KILLS it (verification fails) or it SURVIVES.
set -e
FUNCTION=addRandom
FILE=2048.c
WORK=$(mktemp -d)

# Each mutant: "<lineno>|<new source line>"
# (lines refer to the verified 2048.c)
mutate() {
  ln="$1"; new="$2"
  cp "$FILE" "$WORK/m.c"
  awk -v L="$ln" -v R="$new" 'NR==L{print R; next} {print}' "$FILE" > "$WORK/m.c"
  rm -f "$WORK/m.goto" "$WORK/check.goto"
  if ! goto-cc -o "$WORK/m.goto" "$WORK/m.c" cbmc_stubs.c --function $FUNCTION >/dev/null 2>&1; then
    echo "KILLED (compile error)"; return
  fi
  goto-instrument --partial-loops --unwind 5 "$WORK/m.goto" "$WORK/m.goto" >/dev/null 2>&1
  goto-instrument --enforce-contract $FUNCTION "$WORK/m.goto" "$WORK/check.goto" >/dev/null 2>&1
  if cbmc "$WORK/check.goto" --function $FUNCTION 2>&1 | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "SURVIVED"
  else
    echo "KILLED"
  fi
}

# Find current line numbers of the mutated statements WITHIN addRandom's body.
# addRandom's body starts after its signature/contract; restrict to lines > 340.
first_after() { awk -F: -v M="$2" '$1>M{print $1; exit}'; }
L_LEN=$(grep -n "if (len > 0)" "$FILE" | first_after _ 309)
L_EMPTY=$(grep -n "if (board\[x\]\[y\] == 0)" "$FILE" | first_after _ 309)
L_FY=$(grep -n "for (y = 0; y < SIZE; y++)" "$FILE" | first_after _ 309)
L_FX=$(grep -n "for (x = 0; x < SIZE; x++)" "$FILE" | first_after _ 309)
L_N=$(grep -n "n = (rand() % 10) / 9 + 1;" "$FILE" | first_after _ 309)
L_R=$(grep -n "r = rand() % len;" "$FILE" | first_after _ 309)
echo "lines: len=$L_LEN empty=$L_EMPTY fy=$L_FY fx=$L_FX n=$L_N r=$L_R"

printf "len<0   : "; mutate "$L_LEN" "	if (len < 0)"
printf "len<=0  : "; mutate "$L_LEN" "	if (len <= 0)"
printf "len>=0  : "; mutate "$L_LEN" "	if (len >= 0)"
printf "len==0  : "; mutate "$L_LEN" "	if (len == 0)"
printf "len!=0  : "; mutate "$L_LEN" "	if (len != 0)"
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
printf "n -1    : "; mutate "$L_N" "		n = (rand() % 10) / 9 - 1;"
printf "n *9    : "; mutate "$L_N" "		n = (rand() % 10) * 9 + 1;"
printf "rand*10 : "; mutate "$L_N" "		n = (rand() * 10) / 9 + 1;"
printf "r=rand*l: "; mutate "$L_R" "		r = rand() * len;"

rm -rf "$WORK"
