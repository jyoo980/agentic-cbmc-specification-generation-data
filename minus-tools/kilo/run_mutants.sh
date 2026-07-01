#!/bin/bash
# Test mutation kill score for editorRowHasOpenComment.
# Usage: ./run_mutants.sh <depth>
DEPTH=${1:-300}
FUNCTION=editorRowHasOpenComment
FILE=kilo.c
cp "$FILE" "${FILE}.orig"

# Each mutant is described by line number and sed substitution applied to that line.
declare -a MUTANTS=(
  "388|s@row->render\[row->rsize-1\] != '/'@row->render[row->rsize-1] == '/'@"
  "387|s@row->render\[row->rsize-2\] != '\*'@row->render[row->rsize-2] == '*'@"
  "387|s@row->rsize < 2@row->rsize <= 2@"
  "387|s@row->rsize < 2@row->rsize > 2@"
  "387|s@row->rsize < 2@row->rsize >= 2@"
  "387|s@row->rsize < 2@row->rsize == 2@"
  "387|s@row->rsize < 2@row->rsize != 2@"
  "386|s@row->hl\[row->rsize-1\] == HL_MLCOMMENT@row->hl[row->rsize-1] != HL_MLCOMMENT@"
  "388|s@row->render\[row->rsize-1\] != '/'@row->render[row->rsize+1] != '/'@"
  "387|s@row->render\[row->rsize-2\] != '\*'@row->render[row->rsize+2] != '*'@"
  "386|s@row->hl\[row->rsize-1\] == HL_MLCOMMENT@row->hl[row->rsize+1] == HL_MLCOMMENT@"
  "386|s@HL_MLCOMMENT &&@HL_MLCOMMENT ||@"
  "387|s@row->rsize < 2 ||@row->rsize < 2 \&\&@"
  "387|s@!= '\*' ||@!= '*' \&\&@"
  "386|s@row->rsize \&\&@row->rsize ||@"
  "386|s@row->hl \&\&@row->hl ||@"
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
    echo "MUTANT $i: SED-NOOP (substitution did not apply)"
    continue
  fi
  total=$((total+1))
  rm -f ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto
  out=$(goto-cc -o ${FUNCTION}.goto "$FILE" --function ${FUNCTION} 2>&1 \
    && goto-instrument --partial-loops --unwind 5 ${FUNCTION}.goto ${FUNCTION}.goto 2>&1 \
    && goto-instrument --enforce-contract ${FUNCTION} ${FUNCTION}.goto checking-${FUNCTION}-contracts.goto 2>&1 \
    && cbmc checking-${FUNCTION}-contracts.goto --function ${FUNCTION} --depth ${DEPTH} 2>&1)
  if echo "$out" | grep -q "VERIFICATION SUCCESSFUL"; then
    echo "MUTANT $i (line $line): SURVIVED"
  else
    echo "MUTANT $i (line $line): KILLED"
    killed=$((killed+1))
  fi
done
cp "${FILE}.orig" "$FILE"
rm -f "${FILE}.orig"
echo "=== KILLED $killed / $total ==="
