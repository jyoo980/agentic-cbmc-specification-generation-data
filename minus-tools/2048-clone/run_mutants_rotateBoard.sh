#!/bin/sh
# Mutation testing harness for rotateBoard in 2048.c.
# For each official mutant (a line replacement), apply it, run CBMC enforcement,
# and report whether the spec KILLS it (verification fails) or it SURVIVES.
FUNCTION=rotateBoard
FILE=2048.c
WORK=$(mktemp -d)

# mutate <original-line-text> <mutant-line-text>
mutate() {
  orig="$1"; new="$2"
  # Replace the unique original line with the mutant line.
  awk -v O="$orig" -v R="$new" '$0==O{print R; next} {print}' "$FILE" > "$WORK/m.c"
  if cmp -s "$FILE" "$WORK/m.c"; then echo "NO-OP (line not found!)"; return; fi
  rm -f "$WORK/m.goto" "$WORK/check.goto"
  if ! goto-cc -o "$WORK/m.goto" "$WORK/m.c" cbmc_stubs.c --function $FUNCTION >/dev/null 2>&1; then
    echo "KILLED (compile error)"; return
  fi
  goto-instrument --partial-loops --unwind 5 "$WORK/m.goto" "$WORK/m.goto" >/dev/null 2>&1
  if ! goto-instrument --enforce-contract $FUNCTION "$WORK/m.goto" "$WORK/check.goto" >/dev/null 2>&1; then
    echo "KILLED (instrument error)"; return
  fi
  if cbmc "$WORK/check.goto" --function $FUNCTION --depth 200 2>&1 | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "SURVIVED"
  else
    echo "KILLED"
  fi
}

I='	for (i = 0; i < n / 2; i++)'
J='		for (j = i; j < n - i - 1; j++)'
L222='			board[i][j] = board[j][n - i - 1];'
L223='			board[j][n - i - 1] = board[n - i - 1][n - j - 1];'
L224='			board[n - i - 1][n - j - 1] = board[n - j - 1][i];'
L225='			board[n - j - 1][i] = tmp;'

printf "i<=n/2          : "; mutate "$I" '	for (i = 0; i <= n / 2; i++)'
printf "i>n/2           : "; mutate "$I" '	for (i = 0; i > n / 2; i++)'
printf "i>=n/2          : "; mutate "$I" '	for (i = 0; i >= n / 2; i++)'
printf "i==n/2          : "; mutate "$I" '	for (i = 0; i == n / 2; i++)'
printf "i!=n/2          : "; mutate "$I" '	for (i = 0; i != n / 2; i++)'
printf "i<n*2           : "; mutate "$I" '	for (i = 0; i < n * 2; i++)'

printf "j<=n-i-1        : "; mutate "$J" '		for (j = i; j <= n - i - 1; j++)'
printf "j>n-i-1         : "; mutate "$J" '		for (j = i; j > n - i - 1; j++)'
printf "j>=n-i-1        : "; mutate "$J" '		for (j = i; j >= n - i - 1; j++)'
printf "j==n-i-1        : "; mutate "$J" '		for (j = i; j == n - i - 1; j++)'
printf "j!=n-i-1        : "; mutate "$J" '		for (j = i; j != n - i - 1; j++)'
printf "j<n-i+1         : "; mutate "$J" '		for (j = i; j < n - i + 1; j++)'
printf "j<n+i-1         : "; mutate "$J" '		for (j = i; j < n + i - 1; j++)'

printf "222 j n-i+1     : "; mutate "$L222" '			board[i][j] = board[j][n - i + 1];'
printf "222 j n+i-1     : "; mutate "$L222" '			board[i][j] = board[j][n + i - 1];'

printf "223 rhs n-j+1   : "; mutate "$L223" '			board[j][n - i - 1] = board[n - i - 1][n - j + 1];'
printf "223 rhs n+j-1   : "; mutate "$L223" '			board[j][n - i - 1] = board[n - i - 1][n + j - 1];'
printf "223 rhs n-i+1   : "; mutate "$L223" '			board[j][n - i - 1] = board[n - i + 1][n - j - 1];'
printf "223 rhs n+i-1   : "; mutate "$L223" '			board[j][n - i - 1] = board[n + i - 1][n - j - 1];'
printf "223 lhs n-i+1   : "; mutate "$L223" '			board[j][n - i + 1] = board[n - i - 1][n - j - 1];'
printf "223 lhs n+i-1   : "; mutate "$L223" '			board[j][n + i - 1] = board[n - i - 1][n - j - 1];'

printf "224 rhs n-j+1   : "; mutate "$L224" '			board[n - i - 1][n - j - 1] = board[n - j + 1][i];'
printf "224 rhs n+j-1   : "; mutate "$L224" '			board[n - i - 1][n - j - 1] = board[n + j - 1][i];'
printf "224 lhs n-j+1   : "; mutate "$L224" '			board[n - i - 1][n - j + 1] = board[n - j - 1][i];'
printf "224 lhs n+j-1   : "; mutate "$L224" '			board[n - i - 1][n + j - 1] = board[n - j - 1][i];'
printf "224 lhs n-i+1   : "; mutate "$L224" '			board[n - i + 1][n - j - 1] = board[n - j - 1][i];'
printf "224 lhs n+i-1   : "; mutate "$L224" '			board[n + i - 1][n - j - 1] = board[n - j - 1][i];'

printf "225 n-j+1       : "; mutate "$L225" '			board[n - j + 1][i] = tmp;'
printf "225 n+j-1       : "; mutate "$L225" '			board[n + j - 1][i] = tmp;'

rm -rf "$WORK"
