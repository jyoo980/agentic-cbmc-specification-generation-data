#!/bin/bash
# Test mutation kill score for editorUpdateRow.
# Usage: ./run_mutants_editorUpdateRow.sh [unwind]
UNWIND=${1:-10}
FUNCTION=editorUpdateRow
FILE=kilo.c
cp "$FILE" "${FILE}.orig"

# Each mutant: "line|sed-substitution" applied to that line of kilo.c.
declare -a MUTANTS=(
  # line 654: counting loop bound
  "654|s@j < row->size@j <= row->size@"
  "654|s@j < row->size@j > row->size@"
  "654|s@j < row->size@j >= row->size@"
  "654|s@j < row->size@j == row->size@"
  "654|s@j < row->size@j != row->size@"
  # line 655: tab test in counting loop
  "655|s@row->chars\[j\] == TAB@row->chars[j] != TAB@"
  # line 658: allocsize expression
  "658|s@nonprint\*9 + 1@nonprint*9 - 1@"
  "658|s@+ nonprint\*9@- nonprint*9@"
  "658|s@+ tabs\*8@- tabs*8@"
  # line 659: overflow guard
  "659|s@allocsize > UINT32_MAX@allocsize < UINT32_MAX@"
  "659|s@allocsize > UINT32_MAX@allocsize <= UINT32_MAX@"
  "659|s@allocsize > UINT32_MAX@allocsize >= UINT32_MAX@"
  "659|s@allocsize > UINT32_MAX@allocsize == UINT32_MAX@"
  "659|s@allocsize > UINT32_MAX@allocsize != UINT32_MAX@"
  # line 664: malloc size expression
  "664|s@nonprint\*9 + 1@nonprint*9 - 1@"
  "664|s@+ nonprint\*9@- nonprint*9@"
  "664|s@+ tabs\*8@- tabs*8@"
  # line 666: render loop bound
  "666|s@j < row->size@j <= row->size@"
  "666|s@j < row->size@j > row->size@"
  "666|s@j < row->size@j >= row->size@"
  "666|s@j < row->size@j == row->size@"
  "666|s@j < row->size@j != row->size@"
  # line 667: tab test in render loop
  "667|s@row->chars\[j\] == TAB@row->chars[j] != TAB@"
  # line 669: tab expansion while
  "669|s@(idx+1) % 8 != 0@(idx+1) % 8 == 0@"
  "669|s@(idx+1) % 8 != 0@(idx+1) * 8 != 0@"
  "669|s@(idx+1) % 8 != 0@(idx-1) % 8 != 0@"
)

killed=0
total=0
i=0
for m in "${MUTANTS[@]}"; do
  i=$((i+1))
  line="${m%%|*}"
  sub="${m#*|}"
  cp "${FILE}.orig" "$FILE"
  sed -i "${line}${sub}" "$FILE"
  if diff -q "${FILE}.orig" "$FILE" >/dev/null; then
    echo "MUTANT $i (line $line): SED-NOOP (substitution did not apply)"
    continue
  fi
  total=$((total+1))
  rm -f ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
  out=$(goto-cc -o ${FUNCTION}.goto "$FILE" /app/stubs/ctype.c /app/stubs/string.c --function ${FUNCTION} 2>&1 \
    && goto-instrument --remove-function-body editorUpdateSyntax ${FUNCTION}.goto ${FUNCTION}.goto 2>&1 \
    && goto-cc -o stub_eus.goto stub_editorUpdateSyntax.c 2>&1 \
    && goto-cc -o stub_exit.goto stub_exit.c 2>&1 \
    && goto-cc -o ${FUNCTION}.goto ${FUNCTION}.goto stub_eus.goto stub_exit.goto --function ${FUNCTION} 2>&1 \
    && goto-instrument --unwind ${UNWIND} --unwinding-assertions ${FUNCTION}.goto ${FUNCTION}.goto 2>&1 \
    && goto-instrument --nondet-static --enforce-contract ${FUNCTION} ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto 2>&1 \
    && cbmc checking-${FUNCTION}-contracts.goto --function ${FUNCTION} \
        --no-malloc-may-fail --bounds-check --pointer-check --pointer-primitive-check \
        --pointer-overflow-check --conversion-check --signed-overflow-check --unsigned-overflow-check 2>&1)
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
