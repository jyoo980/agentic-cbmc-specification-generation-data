#!/bin/sh
# Mutation testing harness for getColors in 2048.c.
# For each mutant (a line replacement), apply it, run CBMC enforcement, and
# report whether the spec KILLS it (verification fails) or it SURVIVES.
set -e
FUNCTION=getColors
FILE=2048.c
WORK=$(mktemp -d)

mutate() {
  ln="$1"; new="$2"
  awk -v L="$ln" -v R="$new" 'NR==L{print R; next} {print}' "$FILE" > "$WORK/m.c"
  rm -f "$WORK/m.goto" "$WORK/check.goto"
  if ! goto-cc -o "$WORK/m.goto" "$WORK/m.c" --function $FUNCTION >/dev/null 2>&1; then
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

L_FG=$(grep -n "\*foreground = \*(schemes\[scheme\]" "$FILE" | head -1 | cut -d: -f1)
L_BG=$(grep -n "\*background = \*(schemes\[scheme\]" "$FILE" | head -1 | cut -d: -f1)
echo "lines: fg=$L_FG bg=$L_BG"

printf "bg -  : "; mutate "$L_BG" "	*background = *(schemes[scheme] - (0 + value * 2) % sizeof(original));"
printf "bg *  : "; mutate "$L_BG" "	*background = *(schemes[scheme] + (0 + value * 2) * sizeof(original));"
printf "bg 0- : "; mutate "$L_BG" "	*background = *(schemes[scheme] + (0 - value * 2) % sizeof(original));"
printf "fg -  : "; mutate "$L_FG" "	*foreground = *(schemes[scheme] - (1 + value * 2) % sizeof(original));"
printf "fg *  : "; mutate "$L_FG" "	*foreground = *(schemes[scheme] + (1 + value * 2) * sizeof(original));"
printf "fg 1- : "; mutate "$L_FG" "	*foreground = *(schemes[scheme] + (1 - value * 2) % sizeof(original));"

rm -rf "$WORK"
