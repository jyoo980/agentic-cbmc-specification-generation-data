#!/bin/sh
# Mutation testing harness for slideArray in 2048.c.
# Mirrors the real verification pipeline: replace findTarget with its contract,
# enforce slideArray's contract, run CBMC. Reports KILL/SURVIVE per mutant.
set -e
FUNCTION=slideArray
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
  if ! goto-instrument --replace-call-with-contract findTarget --enforce-contract $FUNCTION "$WORK/m.goto" "$WORK/check.goto" >/dev/null 2>&1; then
    echo "KILLED (instrument error)"; return
  fi
  if cbmc "$WORK/check.goto" --function $FUNCTION --depth 200 2>&1 | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "SURVIVED"
  else
    echo "KILLED"
  fi
}

first_after() { awk -F: -v M="$2" '$1>M{print $1; exit}'; }
SIG=$(grep -n "^bool slideArray" "$FILE" | head -1 | cut -d: -f1)
L_FOR=$(grep -n "for (x = 0; x < SIZE; x++)" "$FILE" | first_after _ "$SIG")
L_AX=$(grep -n "if (array\[x\] != 0)" "$FILE" | first_after _ "$SIG")
L_TX=$(grep -n "if (t != x)" "$FILE" | first_after _ "$SIG")
L_AT0=$(grep -n "if (array\[t\] == 0)" "$FILE" | first_after _ "$SIG")
L_ATX=$(grep -n "else if (array\[t\] == array\[x\])" "$FILE" | first_after _ "$SIG")
L_STOP=$(grep -n "stop = t + 1;" "$FILE" | first_after _ "$SIG")
echo "lines: for=$L_FOR ax=$L_AX tx=$L_TX at0=$L_AT0 atx=$L_ATX stop=$L_STOP"

printf "x<=SIZE      : "; mutate "$L_FOR"  "	for (x = 0; x <= SIZE; x++)"
printf "x>SIZE       : "; mutate "$L_FOR"  "	for (x = 0; x > SIZE; x++)"
printf "x>=SIZE      : "; mutate "$L_FOR"  "	for (x = 0; x >= SIZE; x++)"
printf "x==SIZE      : "; mutate "$L_FOR"  "	for (x = 0; x == SIZE; x++)"
printf "x!=SIZE(eqv) : "; mutate "$L_FOR"  "	for (x = 0; x != SIZE; x++)"
printf "array[x]==0  : "; mutate "$L_AX"   "		if (array[x] == 0)"
printf "t==x         : "; mutate "$L_TX"   "			if (t == x)"
printf "array[t]!=0  : "; mutate "$L_AT0"  "				if (array[t] != 0)"
printf "a[t]!=a[x]   : "; mutate "$L_ATX"  "				else if (array[t] != array[x])"
printf "stop=t-1     : "; mutate "$L_STOP" "					stop = t - 1;"

rm -rf "$WORK"
