#!/bin/sh
# Apply each BoundaryPMFinal mutant to zopfli.c, run verify.sh, report kill/survive.
# A mutant is KILLED if verification FAILS. Restores the file afterward.
set -u
F=zopfli.c
BK=zopfli.c.mutbak
cp "$F" "$BK"

# Each entry: LINE | ORIGINAL | MUTATED
run() {
  line="$1"; orig="$2"; mut="$3"
  cp "$BK" "$F"
  # Replace exact original text on the given line with the mutant text.
  python3 - "$F" "$line" "$orig" "$mut" <<'PY'
import sys
f,line,orig,mut=sys.argv[1],int(sys.argv[2]),sys.argv[3],sys.argv[4]
L=open(f).read().split('\n')
assert orig in L[line-1], f"orig not on line {line}: {L[line-1]!r}"
L[line-1]=L[line-1].replace(orig,mut)
open(f,'w').write('\n'.join(L))
PY
  if ./verify.sh BoundaryPMFinal >/tmp/m.log 2>&1; then
    res="SURVIVED  <-- not killed"
  else
    res="killed"
  fi
  printf '%-6s %-55s %s\n' "$line" "$mut" "$res"
}

# line 399 condition mutants
run 399 "sum > leaves[lastcount].weight" "sum < leaves[lastcount].weight"
run 399 "sum > leaves[lastcount].weight" "sum <= leaves[lastcount].weight"
run 399 "sum > leaves[lastcount].weight" "sum >= leaves[lastcount].weight"
run 399 "sum > leaves[lastcount].weight" "sum == leaves[lastcount].weight"
run 399 "sum > leaves[lastcount].weight" "sum != leaves[lastcount].weight"
run 399 "lastcount < numsymbols" "lastcount <= numsymbols"
run 399 "lastcount < numsymbols" "lastcount > numsymbols"
run 399 "lastcount < numsymbols" "lastcount >= numsymbols"
run 399 "lastcount < numsymbols" "lastcount == numsymbols"
run 399 "lastcount < numsymbols" "lastcount != numsymbols"
run 399 "lastcount < numsymbols &&" "lastcount < numsymbols ||"
# line 410
run 410 "lists[index - 1][1];" "lists[index + 1][1];"
# line 405
run 405 "newchain->count = lastcount + 1;" "newchain->count = lastcount - 1;"
# line 397
run 397 "lists[index - 1][0]->weight + lists[index - 1][1]->weight" "lists[index - 1][0]->weight - lists[index - 1][1]->weight"
run 397 "lists[index - 1][0]->weight + lists[index - 1][1]->weight" "lists[index - 1][0]->weight + lists[index + 1][1]->weight"
run 397 "lists[index - 1][0]->weight + lists[index - 1][1]->weight" "lists[index + 1][0]->weight + lists[index - 1][1]->weight"

cp "$BK" "$F"
rm -f "$BK"
echo "done; file restored"
