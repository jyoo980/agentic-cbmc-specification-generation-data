#!/bin/bash
# Mutation kill-test harness for CalculateBlockSymbolSizeGivenCounts.
# Applies each avocado mutant by replacing a specific line, runs the CBMC
# pipeline, and reports KILLED (verification fails) or SURVIVED (succeeds).
set -u
cd /app/Syzygy_Zopfli/c_code
FUNCTION=CalculateBlockSymbolSizeGivenCounts
SRC=zopfli.c
MUT=__mut_zopfli.c        # kept in source dir so #include "zopfli.h" resolves
WORK=/tmp/cbssgc_mut
mkdir -p "$WORK"

run_pipeline() {
  goto-cc -o "$WORK/m.goto" "$MUT" --function ${FUNCTION} >/dev/null 2>&1 || { echo "BUILDFAIL"; return; }
  goto-instrument --partial-loops --unwind 5 "$WORK/m.goto" "$WORK/m.goto" >/dev/null 2>&1
  goto-instrument --replace-call-with-contract CalculateBlockSymbolSizeSmall \
     --replace-call-with-contract ZopfliGetLengthSymbolExtraBits \
     --replace-call-with-contract ZopfliGetDistSymbolExtraBits \
     --enforce-contract ${FUNCTION} "$WORK/m.goto" "$WORK/c.goto" >/dev/null 2>&1
  local out
  out=$(cbmc "$WORK/c.goto" --function ${FUNCTION} --depth 200 2>&1)
  if echo "$out" | grep -q "VERIFICATION SUCCESSFUL"; then echo "SUCCESS";
  elif echo "$out" | grep -q "VERIFICATION FAILED"; then echo "FAILED";
  else echo "OTHER"; fi
}

# lineno|||replacement-line
declare -a M=(
"385|||    if (lstart + ZOPFLI_NUM_LL * 3 < lend)"
"385|||    if (lstart + ZOPFLI_NUM_LL * 3 <= lend)"
"385|||    if (lstart + ZOPFLI_NUM_LL * 3 >= lend)"
"385|||    if (lstart + ZOPFLI_NUM_LL * 3 == lend)"
"385|||    if (lstart + ZOPFLI_NUM_LL * 3 != lend)"
"385|||    if (lstart - ZOPFLI_NUM_LL * 3 > lend)"
"407|||        for (i = 0; i <= 256; i++)"
"407|||        for (i = 0; i > 256; i++)"
"407|||        for (i = 0; i >= 256; i++)"
"407|||        for (i = 0; i == 256; i++)"
"407|||        for (i = 0; i != 256; i++)"
"411|||        for (i = 257; i <= 286; i++)"
"411|||        for (i = 257; i > 286; i++)"
"411|||        for (i = 257; i >= 286; i++)"
"411|||        for (i = 257; i == 286; i++)"
"411|||        for (i = 257; i != 286; i++)"
"416|||        for (i = 0; i <= 30; i++)"
"416|||        for (i = 0; i > 30; i++)"
"416|||        for (i = 0; i >= 30; i++)"
"416|||        for (i = 0; i == 30; i++)"
"416|||        for (i = 0; i != 30; i++)"
)

killed=0; survived=0; other=0; idx=0
for pair in "${M[@]}"; do
  idx=$((idx+1))
  ln="${pair%%|||*}"
  repl="${pair##*|||}"
  python3 - "$SRC" "$MUT" "$ln" "$repl" <<'PY'
import sys
src,dst,ln,repl=sys.argv[1],sys.argv[2],int(sys.argv[3]),sys.argv[4]
lines=open(src).read().split('\n')
lines[ln-1]=repl
open(dst,'w').write('\n'.join(lines))
PY
  res=$(run_pipeline)
  if [ "$res" == "FAILED" ]; then echo "[$idx] KILLED   : $repl"; killed=$((killed+1));
  elif [ "$res" == "SUCCESS" ]; then echo "[$idx] SURVIVED : $repl"; survived=$((survived+1));
  else echo "[$idx] $res : $repl"; other=$((other+1)); fi
done
rm -f "$MUT"
echo "-----"
echo "KILLED=$killed SURVIVED=$survived OTHER=$other TOTAL=$idx"
