#!/bin/sh
# Mutation testing harness for initBoard in 2048.c.
# Mirrors tools.run_cbmc: goto-cc, --partial-loops --unwind 5, then
# --replace-call-with-contract for the in-file callee (addRandom) +
# --enforce-contract, then cbmc --depth 200. KILLED means verification fails.
set -e
FUNCTION=initBoard
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
  goto-instrument --replace-call-with-contract addRandom \
                  --enforce-contract $FUNCTION "$WORK/m.goto" "$WORK/check.goto" >/dev/null 2>&1
  if cbmc "$WORK/check.goto" --function $FUNCTION --depth 200 2>&1 | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "SURVIVED"
  else
    echo "KILLED"
  fi
}

# Locate initBoard's own loops (the file has many identical loop lines), by
# taking the first x/y loop at or after the function definition.
DEF=$(grep -n "^void initBoard(" "$FILE" | cut -d: -f1)
LX=$(awk -v D="$DEF" 'NR>=D && /for \(x = 0; x < SIZE; x\+\+\)/ {print NR; exit}' "$FILE")
LY=$(awk -v D="$DEF" 'NR>=D && /for \(y = 0; y < SIZE; y\+\+\)/ {print NR; exit}' "$FILE")
echo "x loop at line $LX, y loop at line $LY"

printf "x<=SIZE : "; mutate "$LX" "	for (x = 0; x <= SIZE; x++)"
printf "x>SIZE  : "; mutate "$LX" "	for (x = 0; x > SIZE; x++)"
printf "x>=SIZE : "; mutate "$LX" "	for (x = 0; x >= SIZE; x++)"
printf "x==SIZE : "; mutate "$LX" "	for (x = 0; x == SIZE; x++)"
printf "x!=SIZE : "; mutate "$LX" "	for (x = 0; x != SIZE; x++)"
printf "y<=SIZE : "; mutate "$LY" "		for (y = 0; y <= SIZE; y++)"
printf "y>SIZE  : "; mutate "$LY" "		for (y = 0; y > SIZE; y++)"
printf "y>=SIZE : "; mutate "$LY" "		for (y = 0; y >= SIZE; y++)"
printf "y==SIZE : "; mutate "$LY" "		for (y = 0; y == SIZE; y++)"
printf "y!=SIZE : "; mutate "$LY" "		for (y = 0; y != SIZE; y++)"

rm -rf "$WORK"
