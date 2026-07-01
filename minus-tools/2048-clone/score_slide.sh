#!/bin/sh
# Faithful mutation scorer for slideArray, mirroring the official run_cbmc pipeline
# (goto-cc; goto-instrument --partial-loops --unwind 5; replace findTarget with its
# contract + enforce slideArray; cbmc --depth 200). Usage: sh score_slide.sh <file.c>
set -e
FILE="${1:-2048.c}"
FUNCTION=slideArray
WORK=$(mktemp -d)

mutate() {
  ln="$1"; new="$2"
  awk -v L="$ln" -v R="$new" 'NR==L{print R; next} {print}' "$FILE" > "$WORK/m.c"
  rm -f "$WORK/m.goto" "$WORK/check.goto"
  if ! goto-cc -o "$WORK/m.goto" "$WORK/m.c" --function $FUNCTION >/dev/null 2>&1; then
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

N=0; K=0
report() { N=$((N+1)); printf "%-13s: %s\n" "$1" "$2"; case "$2" in KILLED*) K=$((K+1));; esac; }

report "x<=SIZE"     "$(mutate "$L_FOR"  "	for (x = 0; x <= SIZE; x++)")"
report "x>SIZE"      "$(mutate "$L_FOR"  "	for (x = 0; x > SIZE; x++)")"
report "x>=SIZE"     "$(mutate "$L_FOR"  "	for (x = 0; x >= SIZE; x++)")"
report "x==SIZE"     "$(mutate "$L_FOR"  "	for (x = 0; x == SIZE; x++)")"
report "x!=SIZE(eqv)" "$(mutate "$L_FOR" "	for (x = 0; x != SIZE; x++)")"
report "array[x]==0" "$(mutate "$L_AX"   "		if (array[x] == 0)")"
report "t==x"        "$(mutate "$L_TX"   "			if (t == x)")"
report "array[t]!=0" "$(mutate "$L_AT0"  "				if (array[t] != 0)")"
report "a[t]!=a[x]"  "$(mutate "$L_ATX"  "				else if (array[t] != array[x])")"
report "stop=t-1"    "$(mutate "$L_STOP" "					stop = t - 1;")"

echo "KILL SCORE: $K / $N"
rm -rf "$WORK"
