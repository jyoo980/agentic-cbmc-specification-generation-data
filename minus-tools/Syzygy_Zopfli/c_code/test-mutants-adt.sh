#!/bin/sh
# Apply each AddDynamicTree mutant to zopfli.c, run verify-rec.sh (EncodeTree
# replaced by contract), report kill/survive. KILLED iff verification FAILS.
# Restores the file afterward.
set -u
F=zopfli.c
BK=zopfli.c.mutbak_adt
cp "$F" "$BK"

run() {
  line="$1"; orig="$2"; mut="$3"
  cp "$BK" "$F"
  python3 - "$F" "$line" "$orig" "$mut" <<'PY'
import sys
f,line,orig,mut=sys.argv[1],int(sys.argv[2]),sys.argv[3],sys.argv[4]
L=open(f).read().split('\n')
assert orig in L[line-1], f"orig not on line {line}: {L[line-1]!r}"
L[line-1]=L[line-1].replace(orig,mut)
open(f,'w').write('\n'.join(L))
PY
  if sh verify-rec.sh AddDynamicTree zopfli.c EncodeTree >/tmp/m_adt.log 2>&1; then
    res="SURVIVED  <-- not killed"
  else
    res="killed"
  fi
  printf '%-6s %-45s %s\n' "$line" "$mut" "$res"
}

# line 1803: bestsize/size comparison
run 1823 "size < bestsize" "size <= bestsize"
run 1823 "size < bestsize" "size > bestsize"
run 1823 "size < bestsize" "size >= bestsize"
run 1823 "size < bestsize" "size == bestsize"
run 1823 "size < bestsize" "size != bestsize"
run 1823 "bestsize == 0 ||" "bestsize != 0 ||"
run 1823 "bestsize == 0 ||" "bestsize == 0 &&"
# line 1798: loop bound
run 1818 "i < 8" "i <= 8"
run 1818 "i < 8" "i > 8"
run 1818 "i < 8" "i >= 8"
run 1818 "i < 8" "i == 8"
run 1818 "i < 8" "i != 8"

cp "$BK" "$F"
rm -f "$BK"
echo "done; file restored"
