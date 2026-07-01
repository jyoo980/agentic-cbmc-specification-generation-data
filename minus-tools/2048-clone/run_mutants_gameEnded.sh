#!/bin/sh
# Mutation testing harness for gameEnded in 2048.c.
# Mirrors tools.run_cbmc: goto-cc, --partial-loops --unwind 5, then
# --replace-call-with-contract for each in-file callee + --enforce-contract,
# then cbmc --depth 200. For each mutant, KILLED means verification fails.
set -e
FUNCTION=gameEnded
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
  goto-instrument --replace-call-with-contract countEmpty \
                  --replace-call-with-contract findPairDown \
                  --replace-call-with-contract rotateBoard \
                  --enforce-contract $FUNCTION "$WORK/m.goto" "$WORK/check.goto" >/dev/null 2>&1
  if cbmc "$WORK/check.goto" --function $FUNCTION --depth 200 2>&1 | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "SURVIVED"
  else
    echo "KILLED"
  fi
}

L=$(grep -n "if (countEmpty(board) > 0)" "$FILE" | cut -d: -f1)
echo "countEmpty comparison at line $L"
printf "count<0  : "; mutate "$L" "	if (countEmpty(board) < 0)"
printf "count<=0 : "; mutate "$L" "	if (countEmpty(board) <= 0)"
printf "count>=0 : "; mutate "$L" "	if (countEmpty(board) >= 0)"
printf "count==0 : "; mutate "$L" "	if (countEmpty(board) == 0)"
printf "count!=0 : "; mutate "$L" "	if (countEmpty(board) != 0)"

rm -rf "$WORK"
