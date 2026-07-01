#!/usr/bin/env bash
# Run CBMC contract verification for every function that has been annotated
# with a CBMC contract, one function at a time, and report PASS/FAIL.
#
# Each entry is "FunctionName:LEVEL:UNWIND" where
#   LEVEL = STRONG -> full check set (bounds, pointer, div, pointer-overflow,
#                     conversion, signed+unsigned overflow, float-overflow, nan)
#           SAFE   -> memory-safety + UB subset (bounds, pointer, div,
#                     pointer-overflow, signed-overflow). Used for functions
#                     that rely on intentional unsigned wraparound / narrowing,
#                     where the stricter checks are false positives.
set -u
cd "$(dirname "$0")"

STRONG="--bounds-check --pointer-check --div-by-zero-check --pointer-overflow-check --conversion-check --signed-overflow-check --unsigned-overflow-check --float-overflow-check --nan-check"
SAFE="--bounds-check --pointer-check --div-by-zero-check --pointer-overflow-check --signed-overflow-check"

TARGETS=(
  "AbsDiff:STRONG:6"
  "ZopfliGetLengthSymbol:STRONG:6"
  "ZopfliGetDistSymbol:STRONG:6"
  "ZopfliGetDistSymbolExtraBits:STRONG:6"
  "ZopfliGetLengthSymbolExtraBits:STRONG:6"
  "ZopfliGetLengthExtraBits:STRONG:6"
  "ZopfliGetLengthExtraBitsValue:STRONG:6"
  "ZopfliGetDistExtraBits:STRONG:6"
  "ZopfliGetDistExtraBitsValue:STRONG:6"
  "GetLengthScore:STRONG:6"
  "zopfli_min:STRONG:6"
  "CeilDiv:STRONG:6"
  "GetCostFixed:STRONG:6"
  "PatchDistanceCodesForBuggyDecoders:STRONG:31"
  "InitNode:STRONG:6"
  "InitRanState:STRONG:6"
  "UpdateHashValue:STRONG:6"
  "InitStats:STRONG:6"
  "ClearStatFreqs:STRONG:289"
  "Ran:SAFE:6"
  "ZopfliMaxCachedSublen:SAFE:6"
  "ZopfliLZ77GetByteRange:SAFE:6"
)

pass=0; fail=0
for entry in "${TARGETS[@]}"; do
  fn="${entry%%:*}"; rest="${entry#*:}"; level="${rest%%:*}"; uw="${rest##*:}"
  checks="$STRONG"; [ "$level" = "SAFE" ] && checks="$SAFE"
  out=$(CHECKS="$checks" UNWIND="$uw" ./verify.sh "$fn" 2>&1 | grep -E 'VERIFICATION (SUCCESSFUL|FAILED)' | tail -1)
  if echo "$out" | grep -q SUCCESSFUL; then
    printf 'PASS  %-40s [%s, unwind %s]\n' "$fn" "$level" "$uw"; pass=$((pass+1))
  else
    printf 'FAIL  %-40s [%s, unwind %s] -> %s\n' "$fn" "$level" "$uw" "${out:-no result}"; fail=$((fail+1))
  fi
done
echo "-----"
echo "PASS: $pass  FAIL: $fail"
