#!/bin/bash
# Measure the mutation kill score for is_separator's CBMC specification.
# Usage: ./run_mutants_is_separator.sh [depth]
# Mirrors the verification pipeline (ctype stub + --add-library + enforce-contract).
DEPTH=${1:-200}
FUNCTION=is_separator
FILE=kilo.c
STUB=/app/stubs/ctype.c
cp "$FILE" "${FILE}.orig"

# Each mutant: a sed substitution applied to the body line (line 372).
declare -a MUTANTS=(
  's@strchr(",.()+-/\*=~%\[\];",c) != NULL@strchr(",.()+-/*=~%[];",c) == NULL@'
  's@return c == .\\0.@return c != '"'"'\\0'"'"'@'
  's@isspace(c) || strchr@isspace(c) \&\& strchr@'
  's@c == .\\0. || isspace@c == '"'"'\\0'"'"' \&\& isspace@'
)

killed=0
total=0
i=0
for sub in "${MUTANTS[@]}"; do
  i=$((i+1))
  cp "${FILE}.orig" "$FILE"
  # Apply only on the body return line (the line that starts with "    return c == ").
  sed -i "/^    return c == /${sub}" "$FILE"
  if diff -q "${FILE}.orig" "$FILE" >/dev/null; then
    echo "MUTANT $i: SED-NOOP (substitution did not apply)"
    continue
  fi
  total=$((total+1))
  rm -f ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
  out=$(goto-cc -o ${FUNCTION}.goto "$FILE" "$STUB" --function ${FUNCTION} 2>&1 \
    && goto-instrument --add-library ${FUNCTION}.goto ${FUNCTION}.goto 2>&1 \
    && goto-instrument --partial-loops --unwind 5 ${FUNCTION}.goto ${FUNCTION}.goto 2>&1 \
    && goto-instrument --enforce-contract ${FUNCTION} ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto 2>&1 \
    && cbmc checking-${FUNCTION}-contracts.goto --function ${FUNCTION} --depth ${DEPTH} 2>&1)
  if echo "$out" | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "MUTANT $i: SURVIVED"
  else
    echo "MUTANT $i: KILLED"
    killed=$((killed+1))
  fi
done
cp "${FILE}.orig" "$FILE"
rm -f "${FILE}.orig"
echo "=== KILLED $killed / $total ==="
