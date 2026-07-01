#!/bin/bash
# Strength check for ZopfliBlockSplit: weak-stub the composition-breaking
# callees so the conversion body is reachable, then verify/kill at high depth.
# Usage: _verify_zbs_strength.sh <mode: verify|kill> <depth>
set -u
MODE=${1:-verify}
DEPTH=${2:-600}
SRC=/app/Syzygy_Zopfli/c_code/zopfli.c
FUNCTION=ZopfliBlockSplit
INC=/app/Syzygy_Zopfli/c_code
ALLOC=/app/stubs/cprover_alloc.c
WEAK=/app/stubs/blocksplit_callees.c
WORK=/app/_zbs_strength
# Weak-stubbed (body removed, contract from WEAK); ZopfliBlockSplitLZ77 keeps its real contract.
STUBBED="ZopfliInitLZ77Store ZopfliInitBlockState ZopfliAllocHash ZopfliLZ77Greedy ZopfliCleanBlockState ZopfliCleanLZ77Store ZopfliCleanHash"
REPLACE="--replace-call-with-contract ZopfliBlockSplitLZ77"
for f in $STUBBED; do REPLACE="$REPLACE --replace-call-with-contract $f"; done

build() {
  local src=$1
  rm -f base.goto weak.goto m.goto mc.goto
  goto-cc -o base.goto -I $INC "$src" $ALLOC --function $FUNCTION 2>/tmp/cc.err || { echo CCFAIL; cat /tmp/cc.err; return 1; }
  RM=""; for f in $STUBBED; do RM="$RM --remove-function-body $f"; done
  goto-instrument $RM base.goto base.goto 2>/dev/null
  goto-cc -o weak.goto -I $INC $WEAK 2>/tmp/wk.err || { echo WKFAIL; cat /tmp/wk.err; return 1; }
  goto-cc -o m.goto weak.goto base.goto --function $FUNCTION 2>/tmp/lk.err || { echo LKFAIL; cat /tmp/lk.err; return 1; }
  goto-instrument --partial-loops --unwind 5 m.goto m.goto 2>/dev/null
  goto-instrument $REPLACE --enforce-contract $FUNCTION m.goto mc.goto 2>/tmp/inst.err || { echo INSTFAIL; tail -5 /tmp/inst.err; return 1; }
}

rm -rf $WORK; mkdir -p $WORK; cd $WORK

if [ "$MODE" = "verify" ]; then
  cp $SRC mutated.c
  build mutated.c || exit 1
  cbmc mc.goto --function $FUNCTION --depth $DEPTH 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)|: FAILURE" | tail -20
  exit 0
fi

# kill mode
ORIG=(
'    assert(*npoints == nlz77points);'
'                if (*npoints == nlz77points)'
'            if (lz77splitpoints[*npoints] == i)'
'            size_t length = store.dists[i] == 0 ? 1 : store.litlens[i];'
'        for (i = 0; i < store.size; i++)'
'        for (i = 0; i < store.size; i++)'
'        for (i = 0; i < store.size; i++)'
'        for (i = 0; i < store.size; i++)'
'        for (i = 0; i < store.size; i++)'
'    if (nlz77points > 0)'
'    if (nlz77points > 0)'
'    if (nlz77points > 0)'
'    if (nlz77points > 0)'
)
MUT=(
'    assert(*npoints != nlz77points);'
'                if (*npoints != nlz77points)'
'            if (lz77splitpoints[*npoints] != i)'
'            size_t length = store.dists[i] != 0 ? 1 : store.litlens[i];'
'        for (i = 0; i <= store.size; i++)'
'        for (i = 0; i > store.size; i++)'
'        for (i = 0; i >= store.size; i++)'
'        for (i = 0; i == store.size; i++)'
'        for (i = 0; i != store.size; i++)'
'    if (nlz77points < 0)'
'    if (nlz77points <= 0)'
'    if (nlz77points >= 0)'
'    if (nlz77points == 0)'
)
killed=0
for idx in "${!MUT[@]}"; do
  cp $SRC mutated.c
  O="${ORIG[$idx]}"; M="${MUT[$idx]}"; O="$O" M="$M" perl -i -pe 's/\Q$ENV{O}\E/$ENV{M}/' mutated.c
  if ! grep -qF "$M" mutated.c; then echo "APPLYFAIL $idx"; continue; fi
  if ! build mutated.c >/tmp/b.err 2>&1; then echo "KILLED(build) $idx: $M"; killed=$((killed+1)); continue; fi
  out=$(cbmc mc.goto --function $FUNCTION --depth $DEPTH 2>&1 | grep -E "VERIFICATION (SUCCESSFUL|FAILED)")
  if echo "$out" | grep -q SUCCESSFUL; then echo "SURVIVED  $idx: $M"; else echo "KILLED    $idx: $M"; killed=$((killed+1)); fi
done
echo "=== Killed $killed / ${#MUT[@]} (depth $DEPTH, weak-stub) ==="
