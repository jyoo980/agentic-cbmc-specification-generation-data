---
name: cbmc-getcoststat-distbranch-vacuity
description: "GetCostStat verifies kill 0.25; dist!=0 branch depth-vacuous, dist==0 branch has no mutable operators"
metadata: 
  node_type: memory
  type: project
  originSessionId: 8e850388-047d-47d5-b5c6-b95d8b60fdd5
---

`GetCostStat` (zopfli.c ~2265) verifies SOUNDLY at kill 0.25 (1/4) with a full
functional postcondition for both branches. Twin of [[cbmc-getcostfixed-distbranch-vacuity]]
but capped lower.

**Contract:** requires litlen<=258, dist<=32768, is_fresh(context, sizeof(SymbolStats));
assigns(); ensures dist==0 ==> return == ((SymbolStats*)context)->ll_symbols[litlen];
ensures dist!=0 ==> return == (double)(ZopfliGetLengthExtraBits + ZopfliGetDistExtraBits)
+ ll_symbols[ZopfliGetLengthSymbol(litlen)] + d_symbols[ZopfliGetDistSymbol(dist)].
Helper calls in the ensures are fine — their contracts pin each value uniquely
(even ZopfliGetDistExtraBits's bracket inequalities pin v uniquely per dist).

**NaN gotcha (important):** the dist==0 ensures `return == ll_symbols[litlen]`
FAILS without a guard — havoc gives the fresh SymbolStats doubles a NaN value, and
IEEE NaN!=NaN breaks the self-equality / return-value connection. Fix: add forall
requires that every ll_symbols/d_symbols entry equals itself (`x==x`, true iff not
NaN). Entropy values are always finite, so this is a legitimate domain constraint.
(Note: [[cbmc-copystats-verifies]] used double `==` without this, but it compared two
memcpy'd objects, not a function return value.)

**Why kill is capped at 0.25:** the dist!=0 else-branch is depth-vacuous — the body's
4 contracted-helper replacements exhaust depth 200 before the branch exit is reached,
so its ensures (and the 3 surviving `+ -> -` arithmetic mutants on line 2309) pass
vacuously. CONFIRMED by falsification: a trivial `dist!=0 ==> return==123456.0`
ensures still verifies, and removing the foralls doesn't change it (so the foralls are
NOT the cause — the body's helper-call depth is). The dist==0 branch is reachable but
returns a bare array element with NO mutable operators, so the only killable mutant is
the `if (dist==0)` condition itself (the 1/4 killed). Depth is fixed by the harness;
0.25 is the ceiling.
