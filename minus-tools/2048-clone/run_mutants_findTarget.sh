#!/bin/sh
# Mutation testing harness for findTarget in 2048.c.
# Apply each official mutant, enforce the contract with CBMC, report KILL/SURVIVE.
set -e
FUNCTION=findTarget
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

first_after() { awk -F: -v M="$2" '$1>M{print $1; exit}'; }
SIG=$(grep -n "^uint8_t findTarget" "$FILE" | head -1 | cut -d: -f1)
L_X0=$(grep -n "if (x == 0)" "$FILE" | first_after _ "$SIG")
L_FOR=$(grep -n "for (t = x - 1;; t--)" "$FILE" | first_after _ "$SIG")
L_NZ=$(grep -n "if (array\[t\] != 0)" "$FILE" | first_after _ "$SIG")
L_NE=$(grep -n "if (array\[t\] != array\[x\])" "$FILE" | first_after _ "$SIG")
L_T1=$(grep -n "return t + 1;" "$FILE" | first_after _ "$SIG")
L_ST=$(grep -n "if (t == stop)" "$FILE" | first_after _ "$SIG")
echo "lines: x0=$L_X0 for=$L_FOR nz=$L_NZ ne=$L_NE t1=$L_T1 st=$L_ST"

printf "x!=0        : "; mutate "$L_X0"  "	if (x != 0)"
printf "t=x+1       : "; mutate "$L_FOR" "	for (t = x + 1;; t--)"
printf "array[t]==0 : "; mutate "$L_NZ"  "		if (array[t] == 0)"
printf "a[t]==a[x]  : "; mutate "$L_NE"  "			if (array[t] == array[x])"
printf "return t-1  : "; mutate "$L_T1"  "				return t - 1;"
printf "t!=stop     : "; mutate "$L_ST"  "			if (t != stop)"

rm -rf "$WORK"
