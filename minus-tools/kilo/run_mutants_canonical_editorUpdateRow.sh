#!/bin/bash
# Canonical-pipeline mutation kill score for editorUpdateRow (mirrors tools/run_cbmc.py:
# unwind 5 --partial-loops, depth 200, --replace-call-with-contract editorUpdateSyntax,
# stubs only from /app/stubs, no --nondet-static, CBMC default checks).
# Mutant list is the get-mutants set for this function.
FUNCTION=editorUpdateRow
FILE=kilo.c
cp "$FILE" "${FILE}.orig"

declare -a MUTANTS=(
  "654|s@j < row->size@j <= row->size@"
  "654|s@j < row->size@j > row->size@"
  "654|s@j < row->size@j >= row->size@"
  "654|s@j < row->size@j == row->size@"
  "654|s@j < row->size@j != row->size@"
  "655|s@row->chars\[j\] == TAB@row->chars[j] != TAB@"
  "658|s@nonprint\*9 + 1@nonprint*9 - 1@"
  "658|s@+ nonprint\*9@- nonprint*9@"
  "658|s@+ tabs\*8@- tabs*8@"
  "659|s@allocsize > UINT32_MAX@allocsize < UINT32_MAX@"
  "659|s@allocsize > UINT32_MAX@allocsize <= UINT32_MAX@"
  "659|s@allocsize > UINT32_MAX@allocsize >= UINT32_MAX@"
  "659|s@allocsize > UINT32_MAX@allocsize == UINT32_MAX@"
  "659|s@allocsize > UINT32_MAX@allocsize != UINT32_MAX@"
  "664|s@nonprint\*9 + 1@nonprint*9 - 1@"
  "664|s@+ nonprint\*9@- nonprint*9@"
  "664|s@+ tabs\*8@- tabs*8@"
  "666|s@j < row->size@j <= row->size@"
  "666|s@j < row->size@j > row->size@"
  "666|s@j < row->size@j >= row->size@"
  "666|s@j < row->size@j == row->size@"
  "666|s@j < row->size@j != row->size@"
  "667|s@row->chars\[j\] == TAB@row->chars[j] != TAB@"
  "669|s@(idx+1) % 8 != 0@(idx+1) % 8 == 0@"
  "669|s@(idx+1) % 8 != 0@(idx+1) * 8 != 0@"
  "669|s@(idx+1) % 8 != 0@(idx-1) % 8 != 0@"
)

killed=0
total=0
i=0
G=${FUNCTION}.goto
C=checking-${FUNCTION}-contracts.goto
for m in "${MUTANTS[@]}"; do
  i=$((i+1))
  line="${m%%|*}"
  sub="${m#*|}"
  cp "${FILE}.orig" "$FILE"
  sed -i "${line}${sub}" "$FILE"
  if diff -q "${FILE}.orig" "$FILE" >/dev/null; then
    echo "MUTANT $i (line $line): SED-NOOP"
    continue
  fi
  total=$((total+1))
  rm -f "$G" "$C"
  out=$(goto-cc -o "$G" "$FILE" --function ${FUNCTION} 2>&1 \
    && goto-instrument --partial-loops --unwind 5 "$G" "$G" 2>&1 \
    && goto-instrument --replace-call-with-contract editorUpdateSyntax \
         --enforce-contract ${FUNCTION} "$G" "$C" 2>&1 \
    && cbmc "$C" --function ${FUNCTION} --depth 200 2>&1)
  if echo "$out" | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "MUTANT $i (line $line): SURVIVED  [$sub]"
  else
    echo "MUTANT $i (line $line): KILLED"
    killed=$((killed+1))
  fi
done
cp "${FILE}.orig" "$FILE"
rm -f "${FILE}.orig"
echo "=== KILLED $killed / $total ==="
