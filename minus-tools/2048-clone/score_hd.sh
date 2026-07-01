#!/bin/sh
set -e
FILE="$1"; DEPTH="${2:-800}"; FUNCTION=slideArray
WORK=$(mktemp -d)
mutate() {
  awk -v L="$1" -v R="$2" 'NR==L{print R; next} {print}' "$FILE" > "$WORK/m.c"
  goto-cc -o "$WORK/m.goto" "$WORK/m.c" --function $FUNCTION >/dev/null 2>&1 || { echo KILLED; return; }
  goto-instrument --partial-loops --unwind 5 "$WORK/m.goto" "$WORK/m.goto" >/dev/null 2>&1
  goto-instrument --replace-call-with-contract findTarget --enforce-contract $FUNCTION "$WORK/m.goto" "$WORK/c.goto" >/dev/null 2>&1 || { echo KILLED; return; }
  if cbmc "$WORK/c.goto" --function $FUNCTION --depth $DEPTH 2>&1 | grep -q "VERIFICATION SUCCESSFUL"; then echo SURVIVED; else echo KILLED; fi
}
fa() { awk -F: -v M="$2" '$1>M{print $1; exit}'; }
S=$(grep -n "^bool slideArray" "$FILE"|head -1|cut -d: -f1)
F=$(grep -n "for (x = 0; x < SIZE; x++)" "$FILE"|fa _ "$S")
AX=$(grep -n "if (array\[x\] != 0)" "$FILE"|fa _ "$S")
TX=$(grep -n "if (t != x)" "$FILE"|fa _ "$S")
A0=$(grep -n "if (array\[t\] == 0)" "$FILE"|fa _ "$S")
ATX=$(grep -n "else if (array\[t\] == array\[x\])" "$FILE"|fa _ "$S")
ST=$(grep -n "stop = t + 1;" "$FILE"|fa _ "$S")
K=0;N=0
r(){ N=$((N+1)); printf "%-13s: %s\n" "$1" "$2"; case "$2" in KILLED*) K=$((K+1));; esac; }
r "x<=SIZE"  "$(mutate "$F" "	for (x = 0; x <= SIZE; x++)")"
r "x>SIZE"   "$(mutate "$F" "	for (x = 0; x > SIZE; x++)")"
r "x>=SIZE"  "$(mutate "$F" "	for (x = 0; x >= SIZE; x++)")"
r "x==SIZE"  "$(mutate "$F" "	for (x = 0; x == SIZE; x++)")"
r "x!=SIZE"  "$(mutate "$F" "	for (x = 0; x != SIZE; x++)")"
r "array[x]==0" "$(mutate "$AX" "		if (array[x] == 0)")"
r "t==x"     "$(mutate "$TX" "			if (t == x)")"
r "array[t]!=0" "$(mutate "$A0" "				if (array[t] != 0)")"
r "a[t]!=a[x]" "$(mutate "$ATX" "				else if (array[t] != array[x])")"
r "stop=t-1" "$(mutate "$ST" "					stop = t - 1;")"
echo "KILL ($FILE @depth $DEPTH): $K / $N"
rm -rf "$WORK"
