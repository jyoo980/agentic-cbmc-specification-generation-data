#!/bin/sh
# Mutation testing harness for setBufferedInput in 2048.c.
# Applies each official mutant, runs the grader CBMC pipeline (tools/run_cbmc.py,
# which auto-discovers the termios stubs in /app/stubs), and reports KILLED
# (verification fails on the mutant) or SURVIVED (mutant still verifies).
set -e
FILE=2048.c
FUNCTION=setBufferedInput
WORK=$(mktemp -d)

mutate() {
  pat="$1"; new="$2"
  cp "$FILE" "$WORK/2048.c"
  ln=$(grep -n "$pat" "$FILE" | head -1 | cut -d: -f1)
  awk -v L="$ln" -v R="$new" 'NR==L{print R; next} {print}' "$FILE" > "$WORK/2048.c"
  if python3 /app/tools/run_cbmc.py --function $FUNCTION --file "$WORK/2048.c" 2>&1 \
       | grep -q "verified successfully"; then
    echo "SURVIVED"
  else
    echo "KILLED"
  fi
}

printf "guard1 &&->||: "; mutate "if (enable && !enabled)"      "	if (enable || !enabled)"
printf "guard2 &&->||: "; mutate "else if (!enable && enabled)" "	else if (!enable || enabled)"

rm -rf "$WORK"
